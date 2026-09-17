/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/ir.hpp"

#include "sloejit/arch.hpp"
#include "sloejit/live_range_edit.hpp"
#include "sloejit_assert.hpp"

#include "sloejit/aarch64/aarch64.hpp" // TODO: we shouldn't need this here

#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>

using arch_traits = sloejit::arch_traits;
using block = sloejit::block;
using function = sloejit::function;
using function_options_t = sloejit::function_options_t;
using instruction = sloejit::instruction;
using interval = sloejit::interval;
using live_matrix = sloejit::live_matrix;
using live_position_elem = sloejit::live_position_elem;
using live_positions = sloejit::live_positions;
using live_range = sloejit::live_range;
using live_range_edit = sloejit::live_range_edit;
using reg = sloejit::reg;
using regset = sloejit::regset;
using regset_one_space = sloejit::regset_one_space;
using reloc_info = sloejit::reloc_info;

template <typename T>
using regmap = sloejit::regmap<T>;

#if 0
static std::ostream& operator<<(std::ostream &os, const regset &live) {
  os << " { ";
  for (auto r_id : live) {
    os << r_id << ", ";
  }
  return os << " }";
}
#endif

reg block::fresh_vreg(uint64_t space_id, uint8_t active_mask) {
	return parent->fresh_vreg(space_id, active_mask);
}

reg function::fresh_vreg(uint64_t space_id, uint8_t active_mask) {
	auto id = traits->max_preg_num + gen[space_id]();
	return { space_id, id, active_mask };
}

static inline void insert_or_intersect(std::map<reg, regset> &m, reg r, const regset &rs) {
	// try inserting the specified regset, or else intersect with the existing regset.
	auto [it, inserted] = m.emplace(r, rs);
	if (!inserted) {
		it->second.intersect(rs);
	}
}

void block::update_reg_choices(const arch_traits *t, std::map<reg, regset> &reg_choices) {
	for (auto &instr : instrs) {
		if (instr->have_explicit_reg_choices()) {
			for (unsigned i = 0; i < instr->nregs(); ++i) {
				insert_or_intersect(reg_choices, instr->get_reg(i), instr->get_reg_choices(t, i));
			}
		}
	}
}

bool block::iterate_input_output_set(const arch_traits *t) {
	// iterate backwards, find input set
	regset live = outputs;
	for (auto *instr = instr_last; instr; instr = instr->instr_prev) {
		instr->adjust_input_output_set(t, live);
	}
	t->adjust_special_regs(live);
	return inputs.insert_many(live);
}

template <typename V, typename C, typename K>
static V at_or(const C &c, K k, V v) {
	auto it = c.find(k);
	return it == c.end() ? v : it->second;
}

static void fill_live_positions(block *b, live_positions &lp) {
	for (unsigned i = b->instrs.size(); i-- > 0;) {
		auto &instr = *b->instrs[i];
		for (reg r : instr.output_regs_exact()) {
			lp.emplace(b, r, &instr, false, true, false);
		}
		for (reg r : instr.clobbered_regs()) {
			lp.emplace(b, r, &instr, false, false, true);
		}
		for (reg r : instr.input_regs_exact()) {
			lp.emplace(b, r, &instr, true, false, false);
		}
	}
}

static void fill_live_ranges(block *b, live_positions &lp, live_matrix &live_ranges) {
	// keep track of regs that are never referenced, they still need live ranges
	regset transient_regs = b->outputs;
	lp.map(b, [&](reg r, const std::map<double, live_position_elem> &r_lp) {
		transient_regs.erase(r.space_id, r.id);
		bool have_live_pos = false;
		double live_pos = 0.0;
		if (b->outputs.count(r)) {
			sloejit_assert(!b->instrs.empty());
			// this register was live out of the block.
			have_live_pos = true;
			live_pos = b->instr_last->pos + 1.0;
		}
		for (auto it = r_lp.rbegin(); it != r_lp.rend(); ++it) {
			auto elem = it->second;
			if (elem.is_output && have_live_pos) {
				// this register was produced by this instruction and eventually
				// consumed at live_pos, so begin = it->first, end = live_pos.
				sloejit_assert(have_live_pos && live_pos > it->first);
				live_ranges.emplace(b, r, it->first, live_pos);
				have_live_pos = false;
			}
			else if (elem.is_output || elem.is_clobber) {
				sloejit_assert(!have_live_pos);
				// this register was created and never used, but still technically
				// needs an assignment, so make begin = it->first, end = nextafter(pos).
				auto end = nextafter(it->first, INFINITY);
				live_ranges.emplace(b, r, it->first, end);
			}
			if (elem.is_input) {
				// this might be the last use of this register, so set up the
				// live range to end here if so.
				sloejit_assert(!have_live_pos || live_pos > it->first);
				if (!have_live_pos) {
					have_live_pos = true;
					live_pos = it->first;
				}
			}
		}
		if (have_live_pos) {
			// all remaining registers are block inputs, but still need live
			// ranges, so make begin = 0, end = live_pos.
			live_ranges.emplace(b, r, 0, live_pos);
		}
	});
	for (reg r : transient_regs) {
		// all remaining registers are block inputs (and also never referenced
		// in this particular block) but still need live ranges, so make
		// begin = 0, end = b->instr_last->pos + 1.0.
		live_ranges.emplace(b, r, 0, b->instr_last->pos + 1.0);
	}
}

void block::substitute_constraint_set(const regmap<reg> &subs) {
	// substitute constraint set
	for (auto &i : instrs) {
		i->substitute_constraint_set(subs);
	}
}

static regset_one_space __attribute__((noinline))
apply_physical_subs(const arch_traits *t, const regset_one_space &rs, const regmap<reg> &subs) {
	// apply substitutions but ignore any virtual registers
	regset_one_space ret;
	for (reg r : rs) {
		r.id = subs.at_or(r, r).id;
		// skip if r is a virtual register
		if (r.id > t->max_preg_num) continue;
		ret.insert(r);
	}
	return ret;
}

static void apply_subs(regset &rs, const std::map<reg, uint8_t> &subs) {
	regset ret;
	for (reg r : rs) {
		if (subs.count(r)) r.active_mask = subs.at(r);
		ret.insert(r);
	}
	rs = std::move(ret);
}

static double find_prev_reg_use(const std::map<double, live_position_elem> &r_lp, double end) {
	// find the previous instruction where the register corresponding to this
	// live position entry is used.
	for (auto it = r_lp.rbegin(); it != r_lp.rend(); ++it) {
		auto [pos, e] = *it;
		if (pos < end) {
			// if we are an input, we actually want to check that the reload
			// instruction (that we might place) would overlap, rather than
			// the instruction itself!
			// note this doesn't matter for outputs by virtue of we're only
			// ever searching backwards.
			return e.is_input ? nextafter(pos, 0) : pos;
		}
	}
	// if nothing, we're probably an input to the block, so 0 is correct here.
	return 0;
}

static void spill_reg(block *b, std::vector<live_range_edit> &edits, std::map<reg, reg> &spill_aliases,
                      std::map<reg, std::vector<instruction *>> &spill_details, const arch_traits *t,
                      const std::map<double, live_position_elem> &r_lp, const live_matrix &live_ranges, reg r,
                      reg cause) {

	// replace all writes of r with a write to a new vreg followed by a store
	// replace all reads of r with a load followed by a read of a new vreg
	sloejit_assert(!r_lp.empty());

	auto *cause_lr =
	    at_or<std::pair<block *, const live_range *>>(live_ranges.at(cause), b->id, { nullptr, nullptr })
	        .second;

	// Iterate backwards through the live positions for the spilled register,
	// checking whether we need to spill/reload it at each step. Portions of the
	// spilled register that do not overlap with the cause of the spill can avoid
	// being spilled at each occurrence, and instead just get spilled around the
	// cause itself.

	// Since we are working backwards, keep track of whether we need to emit a
	// store on the next output instruction, and whether a new register
	// (replacing the spilled reg) is currently live (if we are outside of the
	// cause live range).
	bool want_store = b->outputs.count(r);
	double last_use_pos = 0.0;
	bool have_last_use_pos = false;
	std::optional<reg> r_new;

	// the original register that gave rise to the register
	// being spilled e.g. because we spilled this once already.
	auto r_origin = at_or(spill_aliases, r, r);
	auto &r_spills = spill_details[r_origin];

	for (auto it = r_lp.rbegin(); it != r_lp.rend(); ++it) {
		auto *instr = it->second.instr;
		bool r_is_output = instr->is_reg_output(r);
		bool r_is_input = instr->is_reg_input(r);
		sloejit_assert(r_is_input || r_is_output);

		sloejit_assert(r.active_mask > 0);
		if (!have_last_use_pos) {
			r_new = b->fresh_vreg(r.space_id, r.active_mask);
			spill_aliases[*r_new] = r_origin;
		}

		if (r_is_output) {
			sloejit_assert(r_new);
			if (want_store) {
				// this instruction might itself already be a stack reload if we
				// previously spilled here, in which case we can just get rid of
				// it since the store was created by the previous (origin) spill.
				auto r_it = std::find(r_spills.begin(), r_spills.end(), instr);
				if (r_it != r_spills.end()) {
					if (!have_last_use_pos) {
						r_spills.erase(r_it);
						edits.push_back(make_lr_edit_erase_instr(b, instr));
						have_last_use_pos = false;
						want_store = false;
						continue;
					}
					auto end_pos = last_use_pos;
					edits.push_back(make_lr_edit_insert_interval(b, *r_new, { instr->pos, end_pos }));
				}
				else {
					// last_use_pos might still be set here (i.e. we want both a store and
					// a local register containing this value) if it is used immediately
					// and then again later!
					auto *spill_instr = t->emit_spill(b, instr, *r_new);
					spill_details[r_origin].emplace_back(spill_instr);
					// construct new interval from original instruction to the spill instruction,
					// and new position for the register as input to the spill instruction.
					sloejit_assert(!have_last_use_pos || last_use_pos > spill_instr->pos);
					auto end_pos = have_last_use_pos ? last_use_pos : spill_instr->pos;
					sloejit_assert(end_pos > instr->pos);
					edits.push_back(make_lr_edit_insert_interval(b, *r_new, { instr->pos, end_pos }));
					edits.push_back(make_lr_edit_insert_pos(b, *r_new, spill_instr, true, false, false));
				}
			}
			else {
				sloejit_assert(have_last_use_pos);
				edits.push_back(make_lr_edit_insert_interval(b, *r_new, { instr->pos, last_use_pos }));
			}
			have_last_use_pos = false;
			want_store = false;
		}

		if (r_is_input) {
			sloejit_assert(r_new);
			double use_pos = find_prev_reg_use(r_lp, instr->pos);
			interval use_to_self{ use_pos, instr->pos };
			bool want_reload_here = use_pos == 0 || (cause_lr && cause_lr->overlaps(use_to_self));
			want_store = want_store || want_reload_here;
			if (want_reload_here) {
				// this instruction might itself already be a stack spill if we
				// previously spilled here, in which case we can just get rid of
				// it since the store was created by the previous (origin) spill.
				auto r_it = std::find(r_spills.begin(), r_spills.end(), instr);
				if (r_it != r_spills.end()) {
					r_spills.erase(r_it);
					edits.push_back(make_lr_edit_erase_instr(b, instr));
					if (!have_last_use_pos) {
						continue;
					}
				}
				auto *spill_instr = t->emit_reload(b, instr, *r_new);
				spill_details[r_origin].emplace_back(spill_instr);
				// construct new interval from reload instruction to the original instruction.
				// and new position for the register as output from the reload instruction.
				sloejit_assert(instr->pos > spill_instr->pos);
				sloejit_assert(!have_last_use_pos || last_use_pos > instr->pos);
				auto end_pos = have_last_use_pos ? last_use_pos : instr->pos;
				edits.push_back(make_lr_edit_insert_interval(b, *r_new, { spill_instr->pos, end_pos }));
				edits.push_back(make_lr_edit_insert_pos(b, *r_new, spill_instr, false, true, false));
				have_last_use_pos = false;
			}
			else {
				sloejit_assert(!have_last_use_pos || last_use_pos > instr->pos);
				if (!have_last_use_pos) {
					have_last_use_pos = true;
					last_use_pos = instr->pos;
				}
			}
		}

		for (unsigned i = 0; i < instr->nregs(); ++i) {
			sloejit_assert(r_new);
			if (r == instr->get_reg(i)) {
				sloejit_assert(r.active_mask == instr->get_reg(i).active_mask);
				if (instr->have_explicit_reg_choices()) {
					edits.push_back(make_lr_edit_apply_reg_choices(*r_new, instr->get_reg_choices(t, i)));
				}
				instr->set_reg(t, i, *r_new);
			}
		}

		// re-add existing instruction's position data with renamed register
		edits.push_back(make_lr_edit_insert_pos(b, *r_new, instr, r_is_input, r_is_output, false));
	}
}

static std::vector<live_range_edit> spill_reg(function *fn, std::map<reg, reg> &spill_aliases,
                                              std::map<reg, std::vector<instruction *>> &spill_details,
                                              const arch_traits *t, regset &pcs_spilled,
                                              const live_positions &lp, const live_matrix &live_ranges, reg r,
                                              reg cause, block *&prologue) {
	std::vector<live_range_edit> ret;
	bool r_is_input = std::find(fn->inputs.begin(), fn->inputs.end(), r) != fn->inputs.end();
	bool r_is_output = std::find(fn->outputs.begin(), fn->outputs.end(), r) != fn->outputs.end();

	ret.push_back(make_lr_edit_erase_reg(r));

	lp.map(r, [&](block *b, const std::map<double, live_position_elem> &r_lp) {
		::spill_reg(b, ret, spill_aliases, spill_details, t, r_lp, live_ranges, r, cause);
	});

	if (r_is_input && !pcs_spilled.count(r)) {
		// ensure there is a prologue block to add PCS spills to. This block
		// immediately branches to the real entry block and keeps spills out
		// of loop bodies when the first real block could be part of a loop.
		if (!prologue) {
			sloejit_assert(!fn->blocks.empty());
			prologue = fn->make_block("_prologue", 0);
			sloejit::aarch64::instr_builder prologue_ib{ prologue };
			prologue_ib.make_b_i(&*fn->blocks[1]);
		}
		else {
			sloejit_assert(prologue == &*fn->blocks[0]);
			sloejit_assert(!fn->blocks.empty());
		}
		// put spill at start of block (since this is before we set up frame
		// pointers etc). The only other non-spill instruction in this block
		// at this point should be the branch to blocks[1].
		sloejit_assert(fn->blocks[0]->instrs.size() >= 1);
		auto *first_instr = fn->blocks[0]->instr_first;
		auto spill_instr = t->emit_pcs_spill(&*fn->blocks[0], first_instr, r);
		sloejit_assert(spill_instr);
		spill_details[r].emplace_back(spill_instr);
		ret.push_back(make_lr_edit_insert_interval(&*fn->blocks[0], r, { 0, spill_instr->pos }));
		ret.push_back(make_lr_edit_insert_pos(&*fn->blocks[0], r, spill_instr, true, false, false));
	}
	if (r_is_output && !pcs_spilled.count(r)) {
		for (auto &b : fn->blocks) {
			sloejit_assert(b->instrs.size() >= 1);
			auto *last_instr = b->instr_last;
			if (last_instr->base->kind != sloejit::IK_RETURN) {
				continue;
			}
			// put spill immediately before return statement (since this is before
			// we set up frame pointers etc). The only other non-reload instruction
			// in this block at this point should be a "ret" instruction.
			auto spill_instr = t->emit_pcs_reload(&*b, last_instr, r);
			sloejit_assert(spill_instr);
			spill_details[r].emplace_back(spill_instr);
			ret.push_back(make_lr_edit_insert_interval(&*b, r, { spill_instr->pos, last_instr->pos }));
			ret.push_back(make_lr_edit_insert_pos(&*b, r, spill_instr, false, true, false));
		}
	}

	if (r_is_input || r_is_output) {
		pcs_spilled.insert(r);
	}
	return ret;
}

static reg get_stack_spill_reg(const regset_one_space &choices, reg target_r, const live_matrix &live_ranges,
                               const live_positions &lp) {
	reg reg_to_spill;
	double best_cost = std::numeric_limits<double>::max();
	std::map<int, std::pair<block *, const live_range *>> target_lrs = live_ranges.at(target_r);
	for (const auto &elem : target_lrs) {
		const auto &p = elem.second;
		auto *b = p.first;
		auto *target_lr = p.second;
		live_ranges.map_one_space(b, target_r.space_id, [&](block *, reg r, const live_range &lr) {
			if (!choices.count(r)) return;
			double cost = 0.0;
			lp.map(b, r, [&](auto &m) { cost += m.size(); });
			lr.map([&](interval li) {
				if (target_lr->overlaps(li)) {
					// TODO: this is just an arbitrary metric that seems to give decent
					//       register allocation in general, but we will likely want
					//       to revisit this in future.
					cost -= li.end - li.begin;
				}
			});
			if (cost < best_cost) {
				reg_to_spill = r;
				best_cost = cost;
			}
		});
	}
	sloejit_assert(reg_to_spill.id);
	return reg_to_spill;
}

static void apply_lr_edit(regset &edit_regs_added, std::map<reg, regset> &reg_choices,
                          regmap<regset_one_space> &constraints, live_positions &lp, live_matrix &live_ranges,
                          const live_range_edit &edit) {
	switch (edit.kind) {
	case sloejit::live_range_edit_kind::insert_pos: {
		lp.emplace(edit.b, edit.r, edit.instr, edit.is_input, edit.is_output, edit.is_clobber);
		edit_regs_added.insert(edit.r);
		break;
	}
	case sloejit::live_range_edit_kind::insert_interval: {
		live_ranges.emplace(edit.b, edit.r, edit.li);
		auto &constraints_edit_reg = constraints[edit.r];
		live_ranges.map_one_space_overlaps(edit.b, edit.r.space_id, edit.li, [&](reg r) {
			if (r != edit.r) {
				constraints_edit_reg.insert(r);
				constraints[r].insert(edit.r);
			}
		});
		edit_regs_added.insert(edit.r);
		break;
	}
	case sloejit::live_range_edit_kind::apply_reg_choices:
		insert_or_intersect(reg_choices, edit.r, edit.rc);
		break;
	case sloejit::live_range_edit_kind::erase_reg:
		live_ranges.erase(edit.r);
		for (reg r : constraints[edit.r]) {
			constraints[r].erase(edit.r);
		}
		constraints.erase(edit.r);
		break;
	case sloejit::live_range_edit_kind::erase_instr: {
		lp.erase(edit.b, edit.instr);
		edit.b->erase(edit.instr);
		break;
	}
	}
}

static void apply_lr_edits(std::vector<reg> &regs_to_alloc, std::map<reg, regset> &reg_choices,
                           regmap<regset_one_space> &constraints, live_positions &lp,
                           live_matrix &live_ranges, const std::vector<live_range_edit> &edits, reg from) {
	regset edit_regs_added;
	for (auto &edit : edits) {
		apply_lr_edit(edit_regs_added, reg_choices, constraints, lp, live_ranges, edit);
	}
	edit_regs_added.insert(from);
	for (reg r : edit_regs_added) {
		regs_to_alloc.push_back(r);
	}
}

void function::finalize(const stack_frame_info *frame_info) {
	if (finalized) {
		return;
	}

	// construct DAG of blocks to propagate outputs/inputs
	sloejit_assert(!blocks.empty());

	// rename PCS preserved registers into fresh virtual registers to allow
	// register allocation to consider them (even though having another virtual
	// register occupy such a physical register would cause a PCS spill, that is
	// usually preferable to not using the register at all!). We use pcs_vreg_map
	// to keep track of what original physical regs correspond to what fresh
	// virtual regs, so that we can undo the transformation when setting up
	// reg_choices. The order is important here, since the first
	// traits->pcs_preserve.size() registers in the input/output arrays are
	// assumed to be PCS-related.
	std::map<reg, reg> pcs_vreg_map;
	for (auto reg : traits->pcs_preserve) {
		auto vr = fresh_vreg(reg.space_id, reg.active_mask);
		inputs.insert(inputs.begin() + pcs_vreg_map.size(), vr);
		outputs.insert(outputs.begin() + pcs_vreg_map.size(), vr);
		pcs_vreg_map.emplace(vr, reg);
	}

	// TODO: assert that after everything is said and done, this
	//       is actually still true (since we may inadvertently gain
	//       more inputs if we lose a live range somewhere).
	blocks[0]->inputs.insert_many(inputs.begin(), inputs.end());

	// setup block dependencies based on terminating instruction
	for (unsigned i = 0; i < blocks.size(); ++i) {
		auto *b = &*blocks[i];
		sloejit_assert(!b->instrs.empty());
		auto &instr = *b->instr_last;
		switch (instr.base->kind) {
		case sloejit::IK_BRANCH_PCREL: {
			sloejit_assert(instr.targets.size() == 1);
			sloejit_assert(b->successors.size() == 0);
			auto target_b = instr.targets[0]->as_block();
			b->successors.insert(target_b);
			target_b->predecessors.insert(b);
			break;
		}
		case sloejit::IK_COND_BRANCH_PCREL: {
			sloejit_assert(instr.targets.size() == 1);
			sloejit_assert(b->successors.size() == 0);
			sloejit_assert(i < blocks.size() - 1);
			auto target_b = instr.targets[0]->as_block();
			b->successors.insert(target_b);
			b->successors.insert(&*blocks[i + 1]);
			target_b->predecessors.insert(b);
			blocks[i + 1]->predecessors.insert(b);
			break;
		}
		case sloejit::IK_RETURN: sloejit_assert(b->successors.empty()); break;
		default: sloejit_assert(false);
		}
	}

	// setup block dependencies based on terminating instruction
	for (unsigned i = 0; i < blocks.size(); ++i) {
		auto *b = &*blocks[i];
		sloejit_assert(!b->instrs.empty());
		auto &instr = *b->instr_last;
		if (instr.base->kind == sloejit::IK_RETURN) {
			sloejit_assert(b->successors.empty());
			b->outputs.insert_many(outputs.begin(), outputs.end());
		}
	}

	// build input/output regsets for each basic block
	// the first pass is unconditional, but afterwards we can avoid doing
	// anything if the output set didn't change
	for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
		auto *b = it->get();
		sloejit_assert(!b->instrs.empty());
		// block outputs is the union of successor inputs.
		for (auto *succ : b->successors) {
			b->outputs.insert_many(succ->inputs);
		}
		b->iterate_input_output_set(traits);
	}
	for (bool changed = true; changed;) {
		changed = false;
		for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
			auto *b = it->get();
			sloejit_assert(!b->instrs.empty());
			// block outputs is the union of successor inputs.
			bool outputs_changed = false;
			for (auto *succ : b->successors) {
				outputs_changed = b->outputs.insert_many(succ->inputs) || outputs_changed;
			}
			if (outputs_changed) {
				changed = b->iterate_input_output_set(traits) || changed;
			}
		}
	}

	// minimize active mask -- trim lanes we don't care about to reduce
	// spill bandwidth where possible
	std::map<reg, uint8_t> active_masks;
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		for (reg r : b->inputs) {
			auto it = active_masks.find(r);
			if (it == active_masks.end()) {
				active_masks.emplace(r, r.active_mask);
			}
			else {
				it->second &= r.active_mask;
				sloejit_assert(it->second != 0);
			}
		}
		for (reg r : b->outputs) {
			auto it = active_masks.find(r);
			if (it == active_masks.end()) {
				active_masks.emplace(r, r.active_mask);
			}
			else {
				it->second &= r.active_mask;
				sloejit_assert(it->second != 0);
			}
		}
		for (auto &ins : b->instrs) {
			for (unsigned i = 0; i < ins->nregs(); ++i) {
				reg r = ins->get_reg(i);
				auto it = active_masks.find(r);
				if (it == active_masks.end()) {
					active_masks.emplace(r, r.active_mask);
				}
				else {
					it->second &= r.active_mask;
					sloejit_assert(it->second != 0);
				}
			}
		}
	}
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		apply_subs(b->inputs, active_masks);
		apply_subs(b->outputs, active_masks);
		for (auto &ins : b->instrs) {
			for (unsigned i = 0; i < ins->nregs(); ++i) {
				reg r = ins->get_reg(i);
				r.active_mask = active_masks.at(r);
				sloejit_assert(r.active_mask != 0);
				ins->set_reg(traits, i, r);
			}
		}
	}

	// renumber instr->pos values to avoid successive instructions having the
	// same position when we repeatedly insert before other instructions (since
	// make_instr takes the midpoint of the two instructions it inserts between
	// this can eventually run out of floating-point mantissa bits).
	for (auto &b : blocks) {
		int new_pos = 1;
		for (auto *instr = b->instr_first; instr; instr = instr->instr_next) {
			instr->pos = new_pos++;
		}
	}

	live_positions lp;
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		fill_live_positions(&*b, lp);
	}

	// build live ranges -- no need to iterate here since convergence
	// is guaranteed by input/output set iteration above
	live_matrix live_ranges;
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		fill_live_ranges(&*b, lp, live_ranges);
	}

	// build constraint set from live ranges
	// warning: this is O(n^2) in the number of variables, not great!
	regmap<regset_one_space> constraints;
	live_ranges.map([&](block *b1, reg r1, const live_range &lr1) {
		auto &constraints_r1 = constraints[r1];
		live_ranges.map_one_space_overlaps(b1, r1.space_id, lr1, [&constraints_r1, r1](reg r2) {
			if (r1 != r2) {
				constraints_r1.insert(r2);
			}
		});
	});

	// (set of preg choices for particular vregs, namely inputs/outputs)
	// choices for all regs not in this set is given by (*traits->regs_for_space)(r.space_id).
	std::map<reg, regset> reg_choices = traits->get_pcs_reg_choices(pcs_vreg_map, inputs, outputs);
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		b->update_reg_choices(traits, reg_choices);
	}
	for (auto &p : reg_choices) {
		// if this fails then we have ended up with conflicting reg choices!
		sloejit_assert(!p.second.empty());
	}

	std::vector<std::vector<reg>> reg_sequence_groups;
	std::map<reg, size_t> reg_sequence_lookup;
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		for (auto &instr : b->instrs) {
			for (const auto &seq : instr->get_reg_sequences()) {
				sloejit_assert(seq.count > 1);
				sloejit_assert(seq.start_index + seq.count <= instr->nregs());
				std::vector<reg> group(seq.count);
				reg base_reg = instr->get_reg(seq.start_index);
				for (size_t i = 0; i < seq.count; ++i) {
					reg r = instr->get_reg(seq.start_index + i);
					sloejit_assert(r.space_id == base_reg.space_id);
					group[i] = r;
				}
				auto group_index = reg_sequence_groups.size();
				auto cur_group = reg_sequence_groups.emplace_back(std::move(group));
				for (size_t i = 0; i < cur_group.size(); ++i) {
					reg r = cur_group[i];
					sloejit_assert(reg_sequence_lookup.emplace(r, group_index).second);
				}
			}
		}
	}

	// prefer to allocate registers with a larger live range earlier.
	std::map<reg, double> regs_weighting_rev;
	live_ranges.map([&regs_weighting_rev, this](block *, reg r, const live_range &lr) {
		if (!traits->reg_is_virtual(r)) return;
		double len = 0;
		lr.map([&](interval li) { len += li.end - li.begin; });
		regs_weighting_rev[r] -= len;
	});
	std::map<double, std::vector<reg>> regs_weighting;
	for (auto &p : regs_weighting_rev) {
		regs_weighting[p.second].push_back(p.first);
	}
	std::vector<reg> regs_to_alloc;
	for (auto &p : regs_weighting) {
		for (reg r : p.second) {
			sloejit_assert(r.active_mask != 0);
			regs_to_alloc.push_back(r);
		}
	}

	std::vector<bool> reg_sequence_allocated(reg_sequence_groups.size(), false);

	regset pcs_spilled;
	regmap<reg> subs;

	// we also keep track of the live range of spilled registers,
	// which can be used to try and color stack spill slots in
	// `finalize_spills`.
	live_matrix spill_ranges;
	std::map<reg, std::vector<instruction *>> spill_details;
	std::map<reg, reg> spill_aliases;
	block *prologue = nullptr;
	if (opts.keep_frame_pointer) {
		prologue = make_block("_prologue", 0);
		sloejit::aarch64::instr_builder prologue_ib{ prologue };
		prologue_ib.make_b_i(&*blocks[1]);
	}

	// solve constraint set
	for (unsigned alloc_idx = 0; alloc_idx < regs_to_alloc.size(); ++alloc_idx) {
		auto from = regs_to_alloc[alloc_idx];
		sloejit_assert(traits->reg_is_virtual(from));
		if (subs.count(from)) {
			continue;
		}
		auto seq_it = reg_sequence_lookup.find(from);
		if (seq_it != reg_sequence_lookup.end()) {
			auto seq_index = seq_it->second;
			if (reg_sequence_allocated[seq_index]) {
				continue;
			}
			const auto &seq_group = reg_sequence_groups[seq_index];
			sloejit_assert(!seq_group.empty());
			auto space_id = seq_group.front().space_id;
			regset_one_space available_regs = traits->regs_for_space(space_id);
			traits->erase_special_regs(available_regs);
			std::vector<reg> available_list(available_regs.begin(), available_regs.end());
			sloejit_assert(!available_list.empty());
			bool allocated = false;
			for (size_t base_idx = 0; base_idx < available_list.size(); ++base_idx) {
				bool ok = true;
				for (size_t offset = 0; offset < seq_group.size(); ++offset) {
					reg vreg = seq_group[offset];
					reg candidate = available_list[(base_idx + offset) % available_list.size()];
					reg fixed = subs.at_or(vreg, vreg);
					if (!traits->reg_is_virtual(fixed)) {
						if (fixed.id != candidate.id) {
							ok = false;
							break;
						}
						continue;
					}
					if ((candidate.active_mask & vreg.active_mask) != vreg.active_mask) {
						ok = false;
						break;
					}
					auto choices_it = reg_choices.find(vreg);
					if (choices_it != reg_choices.end() && !choices_it->second.count(candidate)) {
						ok = false;
						break;
					}
					if (constraints.count(vreg)) {
						auto p_interference_set = apply_physical_subs(traits, constraints.at(vreg), subs);
						if (p_interference_set.count(candidate)) {
							ok = false;
							break;
						}
					}
				}
				if (!ok) {
					continue;
				}
				for (size_t offset = 0; offset < seq_group.size(); ++offset) {
					reg vreg = seq_group[offset];
					reg candidate = available_list[(base_idx + offset) % available_list.size()];
					if (traits->reg_is_virtual(vreg)) {
						if (subs.count(vreg)) {
							sloejit_assert(subs.at(vreg).id == candidate.id);
						}
						else {
							subs.insert(vreg, candidate);
						}
					}
				}
				allocated = true;
				reg_sequence_allocated[seq_index] = true;
				break;
			}
			if (allocated) {
				continue;
			}
			regset_one_space spill_choices;
			for (reg vreg : seq_group) {
				if (constraints.count(vreg)) {
					spill_choices.insert_many(constraints.at(vreg));
				}
			}
			spill_choices.erase_many(traits->regs_for_space(space_id));
			for (reg vreg : seq_group) {
				spill_choices.insert(vreg);
			}
			traits->erase_special_regs(spill_choices);
			reg reg_to_spill = get_stack_spill_reg(spill_choices, from, live_ranges, lp);
			sloejit_assert(reg_to_spill.id);
			auto lr_edits = ::spill_reg(this, spill_aliases, spill_details, traits, pcs_spilled, lp,
			                            live_ranges, reg_to_spill, from, prologue);
			sloejit_assert(!lr_edits.empty());
			std::map<block *, const live_range *> spill_lrs;
			const auto &reg_to_spill_lrs = live_ranges.at(reg_to_spill);
			for (const auto &elem : reg_to_spill_lrs) {
				spill_lrs.emplace(elem.second);
			}
			spill_ranges.emplace(reg_to_spill, std::move(spill_lrs));
			apply_lr_edits(regs_to_alloc, reg_choices, constraints, lp, live_ranges, std::move(lr_edits),
			               from);
			subs.erase(reg_to_spill);
			continue;
		}

		// get the interference set for this register. note that this might be
		// empty (if this register conflicts with absolutely nothing) or even
		// not present (if this register was previously spilled).
		if (constraints.count(from) == 0) {
			continue;
		}
		const auto &interference_set = constraints.at(from);

		// we only care about the physical regs here (since that's what
		// we're erasing from) so don't bother with other regs.
		auto p_interference_set = apply_physical_subs(traits, interference_set, subs);

		// what (substituted) regs do we have to choose from.
		// note we also keep around the choices for later in case we need
		// to spill something, we don't always need this since in many
		// cases we don't care what register we end up with.
		std::optional<regset_one_space> from_choices_if_explicit;
		auto from_choices_it = reg_choices.find(from);
		regset_one_space choices;
		if (from_choices_it != reg_choices.end()) {
			choices = { from_choices_it->second.begin(), from_choices_it->second.end() };
			from_choices_if_explicit = { from_choices_it->second.begin(), from_choices_it->second.end() };
		}
		else {
			choices = traits->regs_for_space(from.space_id);
		}
		choices.erase_many(p_interference_set);
		traits->erase_special_regs(choices);

		// if we have any valid choices, arbitrarily pick one of them
		auto to = std::find_if(choices.begin(), choices.end(),
		                       [&](reg c) { return (c.active_mask & from.active_mask) == from.active_mask; });
		if (to != choices.end()) {
			// update substitutions
			subs.insert(from, *to);
			continue;
		}

		// no choice, give up, spill register and retry
		// our choices of are either any (unsubbed) vregs from the constraint set, or ourselves.
		auto choices_init = interference_set;
		choices_init.erase_many(traits->regs_for_space(from.space_id));
		if (!from_choices_if_explicit) {
			choices = std::move(choices_init);
		}
		else {
			choices.clear();
			for (reg rb : choices_init) {
				// don't bother spilling something that wouldn't actually ever conflict due to disjoint
				// reg_choices
				auto rb_choices_it = reg_choices.find(rb);
				if (rb_choices_it == reg_choices.end()) {
					choices.insert(rb);
				}
				else {
					regset_one_space rb_pregs = { rb_choices_it->second.begin(),
						                          rb_choices_it->second.end() };
					rb_pregs.intersect(*from_choices_if_explicit);
					if (!rb_pregs.empty()) {
						choices.insert(rb);
					}
				}
			}
		}
		choices.insert(from);
		traits->erase_special_regs(choices);
		reg reg_to_spill = get_stack_spill_reg(choices, from, live_ranges, lp);
		sloejit_assert(reg_to_spill.id);
		auto lr_edits = ::spill_reg(this, spill_aliases, spill_details, traits, pcs_spilled, lp, live_ranges,
		                            reg_to_spill, from, prologue);
		sloejit_assert(!lr_edits.empty());
		// TODO: the spilled range is almost exactly the same as the original
		//       live range, except it should be based around the load/store
		//       instructions rather than the def/use of the original vreg.
		//       this is fine for now since it is just an overapproximation,
		//       but will become problematic if we ever try to do better than
		//       just spilling all occurrences of a vreg when we spill it.
		std::map<block *, const live_range *> spill_lrs;
		const auto &reg_to_spill_lrs = live_ranges.at(reg_to_spill);
		for (const auto &elem : reg_to_spill_lrs) {
			spill_lrs.emplace(elem.second);
		}
		spill_ranges.emplace(reg_to_spill, std::move(spill_lrs));
		apply_lr_edits(regs_to_alloc, reg_choices, constraints, lp, live_ranges, std::move(lr_edits), from);
		subs.erase(reg_to_spill);
	}

	traits->finalize_spills(this, frame_info, spill_details, spill_ranges, prologue);

	// actually do the substitution
	for (auto &b : blocks) {
		b->substitute_constraint_set(subs);
	}

	traits->post_regalloc_hook(this);

	// remove unused instructions
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		for (unsigned i = b->instrs.size(); i-- > 0;) {
			auto *instr = &*b->instrs[i];
			for (unsigned j = 0; j < instr->nregs(); ++j) {
				auto r = instr->get_reg(j);
				if (traits->reg_is_virtual(r)) {
					fprintf(stderr, "erasing instruction %u/%lf due to virtual register (%u, %u, 0x%x)\n", i,
					        instr->pos, r.space_id, (unsigned) r.id, r.active_mask);
					sloejit_assert(false);
				}
			}
		}
	}

	// substitute addresses for targets
	int64_t cur_ofs = 0;
	std::map<sloejit::branch_target *, int64_t> offsets;
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		offsets[&*b] = cur_ofs;
		// TODO: this is hardcoded for aarch64, which is 4 bytes per instr.
		cur_ofs += b->instrs.size() * 4;
	}
	for (auto &b : blocks) {
		sloejit_assert(!b->instrs.empty());
		int ofs = 0;
		for (auto *instr = b->instr_first; instr; instr = instr->instr_next, ++ofs) {
			for (auto *tb : instr->targets) {
				// TODO: this is hardcoded for aarch64, which is 4 bytes per instr.
				int64_t base = offsets.at(&*b) + ofs * 4;
				auto it = offsets.find(tb);
				if (it != offsets.end()) {
					instr->literals.push_back(it->second - base);
				}
			}
		}
	}
	finalized = true;
}
