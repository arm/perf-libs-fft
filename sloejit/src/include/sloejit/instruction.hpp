/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "arch.hpp"
#include "branch_target.hpp"
#include "bytevector.hpp"
#include "function_options.hpp"
#include "gtelf.hpp"
#include "regmap.hpp"
#include "regset.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace sloejit {

struct instruction;
using instr_ptr = std::unique_ptr<instruction>;

/// Emit an instruction in a binary format.
using emit_bin_t = void (*)(bytevector &bv, const instruction &instr, int64_t ofs,
                            std::vector<reloc_info> *relocs);

/// Emit an instruction in a string format.
using emit_asm_t = std::string (*)(const instruction &instr);

/**
 * This is used to indicate whether an instruction alters control flow,
 * and if so then how. Blocks of instructions must have only normal instructions
 * up until the last instruction, and must be terminated by a non-normal instruction.
 * This is validated at emit time.
 */
enum instr_kind_t {
	IK_NORMAL, ///< Instruction does not alter control flow
	IK_BRANCH_PCREL, ///< Instruction is a branch to target
	IK_COND_BRANCH_PCREL, ///< Instruction is a conditional branch to target
	IK_RETURN, ///< Instruction is a return from a function
};

/**
 * A base class for instruction invariants.
 */
struct instr_base {
	int opcode; ///< The opcode for this instruction (see xxxx_opcodes.hpp)
	instr_kind_t kind; ///< Whether this instruction alters control flow, and how.
	std::vector<bool> base_input_mask; ///< Which registers are inputs to this instruction.
	std::vector<bool> base_output_mask; ///< Which registers are outputs from this instruction.
	std::vector<uint8_t> base_input_active_mask; ///< What lanes are consumed by this instruction.
	std::vector<uint8_t> base_output_active_mask; ///< What lanes are set (or zeroed) by this instruction.
	regset clobbered; ///< What registers are clobbered by this instruction.

	/**
	 * A function pointer, called to write the binary representation of an
	 * instance of this instruction. May be nullptr for virtual instructions
	 * which are removed before emission.
	 */
	emit_bin_t emit_bin;

	/**
	 * A function pointer, called to write the string representation of an
	 * instance of this instruction. May be nullptr, in which case a fallback is
	 * used.
	 */
	emit_asm_t emit_asm;

	instr_base(int opcode, decltype(kind) kind, decltype(base_input_mask) base_input_mask,
	           decltype(base_output_mask) base_output_mask,
	           decltype(base_input_active_mask) base_input_active_mask,
	           decltype(base_output_active_mask) base_output_active_mask, decltype(clobbered) clobbered,
	           decltype(emit_bin) emit_bin, decltype(emit_asm) emit_asm)
	    : opcode(opcode), kind(kind), base_input_mask(std::move(base_input_mask)),
	      base_output_mask(std::move(base_output_mask)),
	      base_input_active_mask(std::move(base_input_active_mask)),
	      base_output_active_mask(std::move(base_output_active_mask)), clobbered(std::move(clobbered)),
	      emit_bin(emit_bin), emit_asm(emit_asm) {
		sloejit_assert(this->base_input_mask.size() == this->base_input_active_mask.size());
		sloejit_assert(this->base_output_mask.size() == this->base_output_active_mask.size());
	}
};

struct reg_sequence {
	uint8_t start_index;
	uint8_t count;
};

/**
 * An instance of a machine instruction for the target architecture.
 * Belongs to a particular basic block (parent).
 */
struct instruction {
	/// The basic block owning this instruction.
	block *parent;

	/// A pointer to the base class for this instruction.
	const instr_base *base;

	/// The position of this instruction, this must be monotonically
	/// increasing within the parent basic block, but is otherwise
	/// meaningless (We use this in the construction of live ranges).
	double pos;

	std::optional<std::string> tag;

	instruction *instr_next;
	instruction *instr_prev;

private:
	std::vector<reg> regs;

	/// Additional input registers to this instruction.
	/// This may be non-empty if this is a variadic instruction
	/// (e.g. if this is a function call).
	std::vector<bool> extra_input_mask;

	/// Additional output registers from this instruction.
	/// This may be non-empty if this is a variadic instruction
	/// (e.g. if this is a function call).
	std::vector<bool> extra_output_mask;

	/// Which lanes are consumed by this instruction.
	/// This may be longer than the base mask if this is variadic
	/// (e.g. if this is a function call).
	std::vector<uint8_t> extra_input_active_mask;

	/// Which lanes are set (or zeroed) by this instruction.
	/// This may be longer than the base mask if this is variadic
	/// (e.g. if this is a function call).
	std::vector<uint8_t> extra_output_active_mask;

	/// The set of valid assignments for this register. This will ordinarily
	/// be either (if a virtual register) the set of registers for a space or
	/// (if a physical register) that particular register!
	/// This must either be empty or have .size() == regs.size().
	/// (e.g. if only one reg has constraints, the others must still be filled
	///       with the set of the registers for that space).
	std::vector<regset> reg_choices;

	/// Sequences of registers that must be allocated consecutively.
	std::vector<reg_sequence> reg_sequences;

public:
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;

	/// Create an instruction with explicit additional input/output masks and register choices.
	instruction(decltype(parent) parent, decltype(base) base, decltype(pos) pos, decltype(tag) tag,
	            decltype(regs) regs, decltype(extra_input_mask) extra_input_mask,
	            decltype(extra_output_mask) extra_output_mask,
	            decltype(extra_input_active_mask) extra_input_active_mask,
	            decltype(extra_output_active_mask) extra_output_active_mask,
	            decltype(reg_choices) reg_choices, decltype(literals) literals, decltype(targets) targets)
	    : parent(parent), base(base), pos(pos), tag(std::move(tag)), regs(std::move(regs)),
	      extra_input_mask(std::move(extra_input_mask)), extra_output_mask(std::move(extra_output_mask)),
	      extra_input_active_mask(std::move(extra_input_active_mask)),
	      extra_output_active_mask(std::move(extra_output_active_mask)), reg_choices(std::move(reg_choices)),
	      literals(std::move(literals)), targets(std::move(targets)) {
		sloejit_assert(this->extra_input_mask.size() == this->extra_input_active_mask.size());
		sloejit_assert(this->extra_output_mask.size() == this->extra_output_active_mask.size());
	}

	/// Create an instruction with only the base input/output masks.
	instruction(decltype(parent) parent, decltype(base) base, decltype(pos) pos, decltype(tag) tag,
	            decltype(regs) regs, decltype(literals) literals, decltype(targets) targets)
	    : parent(parent), base(base), pos(pos), tag(std::move(tag)), regs(std::move(regs)),
	      literals(std::move(literals)), targets(std::move(targets)) {
	}

	/// Create an instruction with a sequence constraint.
	instruction(decltype(parent) parent, decltype(base) base, decltype(pos) pos, decltype(tag) tag,
	            decltype(regs) regs, decltype(literals) literals, decltype(targets) targets,
	            decltype(reg_sequences) reg_sequences)
	    : parent(parent), base(base), pos(pos), tag(std::move(tag)), regs(std::move(regs)),
	      reg_sequences(std::move(reg_sequences)), literals(std::move(literals)),
	      targets(std::move(targets)) {
	}

	inline unsigned nregs() const {
		return regs.size();
	}

	inline reg get_reg(int i) const {
		return regs.at(i);
	}

	template <int... Is>
	inline std::array<reg, sizeof...(Is)> get_regs() const {
		static_assert(
		    sizeof...(Is) != 0,
		    "get_regs takes template parameters of the indices to extract, but none were specified");
		return { regs.at(Is)... };
	}

	regset get_reg_choices(const arch_traits *t, int i) const {
		if (reg_choices.empty()) {
			auto ri_rc = t->regs_for_space(get_reg(i).space_id);
			sloejit_assert(!ri_rc.empty());
			return { ri_rc.begin(), ri_rc.end() };
		}
		return reg_choices.at(i);
	}

	bool have_explicit_reg_choices() const {
		return !reg_choices.empty();
	}

	const std::vector<reg_sequence> &get_reg_sequences() const {
		return reg_sequences;
	}

	inline void set_reg(const arch_traits *t, int i, reg r) {
		sloejit_assert(t->reg_is_virtual(r) || get_reg_choices(t, i).count(r));
		regs[i] = r;
	}

	inline bool is_reg_input(unsigned i) const {
		if (i < base->base_input_mask.size()) {
			return base->base_input_mask[i];
		}
		return extra_input_mask.at(i - base->base_input_mask.size());
	}

	inline int num_output_regs() const {
		int ret = 0;
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_output(i)) {
				++ret;
			}
		}
		return ret;
	}

	inline bool is_reg_output(unsigned i) const {
		if (i < base->base_output_mask.size()) {
			return base->base_output_mask[i];
		}
		return extra_output_mask.at(i - base->base_output_mask.size());
	}

	inline bool is_reg_input(reg r) const {
		for (unsigned i = 0; i < regs.size(); ++i) {
			if (is_reg_input(i) && r == get_reg(i)) {
				return true;
			}
		}
		return false;
	}

	inline bool is_reg_output(reg r) const {
		for (unsigned i = 0; i < regs.size(); ++i) {
			if (is_reg_output(i) && r == get_reg(i)) {
				return true;
			}
		}
		return false;
	}

	inline uint8_t get_reg_input_active_mask(unsigned i) const {
		sloejit_assert(is_reg_input(i));
		if (i < base->base_input_active_mask.size()) {
			return base->base_input_active_mask[i];
		}
		return extra_input_active_mask.at(i - base->base_input_active_mask.size());
	}

	inline uint8_t get_reg_output_active_mask(unsigned i) const {
		sloejit_assert(is_reg_output(i));
		if (i < base->base_output_active_mask.size()) {
			return base->base_output_active_mask[i];
		}
		return extra_output_active_mask.at(i - base->base_output_active_mask.size());
	}

	inline int64_t get_literal(int i) const {
		return literals.at(i);
	}

	inline void substitute_constraint_set(const regmap<reg> &subs) {
		for (unsigned i = 0; i < regs.size(); ++i) {
			auto r = regs[i];
			sloejit_assert(r.active_mask > 0);
			regs[i].id = subs.at_or(r, r).id;
		}
	}

	inline void substitute_reg(reg from, reg to) {
		for (unsigned i = 0; i < regs.size(); ++i) {
			if (regs[i] == from) {
				regs[i] = to;
			}
		}
	}

	inline void narrow_output_active_mask(const regset &live) {
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_output(i)) {
				regs[i] = live.at_or_narrower(regs[i]);
			}
		}
	}

	/**
	 * Get a register set representing the exact input register set.
	 */
	inline regset input_regs_exact() const {
		regset ret;
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_input(i)) {
				ret.insert(get_reg(i));
			}
		}
		return ret;
	}

	/**
	 * Insert all input registers of this instruction into the specified regset.
	 *
	 * @param[in,out] rs The regset to insert into.
	 */
	inline void insert_input_regs_exact(regset &rs) const {
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_input(i)) {
				rs.insert(get_reg(i));
			}
		}
	}

	/**
	 * Get a register set representing the widest possible output
	 * register set. Many instructions zero the upper portion of their
	 * destination register, and hence this will be set even if we never
	 * actually read that particular bit of the register.
	 */
	inline regset output_regs_wide() const {
		regset ret;
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_output(i)) {
				// adjust active mask to be that given by the instruction
				// rather than the register itself (which is a subset).
				reg ri = get_reg(i);
				auto ri_mask = get_reg_output_active_mask(i);
				sloejit_assert((ri_mask | ri.active_mask) == ri_mask);
				ri.active_mask = ri_mask;
				ret.insert(ri);
			}
		}
		return ret;
	}

	/**
	 * Erase from a register set the widest possible active mask for each output
	 * register of this instruction.  Many instructions zero the upper portion of
	 * their destination register, and hence this will be set even if we never
	 * actually read that particular bit of the register.
	 *
	 * @param[in,out] rs The regset to erase from.
	 */
	inline void erase_output_regs_wide(regset &rs) const {
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_output(i)) {
				// adjust active mask to be that given by the instruction
				// rather than the register itself (which is a subset).
				reg ri = get_reg(i);
				auto ri_mask = get_reg_output_active_mask(i);
				sloejit_assert((ri_mask | ri.active_mask) == ri_mask);
				ri.active_mask = ri_mask;
				rs.erase(ri);
			}
		}
	}

	/**
	 * Get a register set representing the exact output
	 * register set. Many instructions zero the upper portion of their
	 * destination register, however we will ignore this and any regions
	 * that we never actually read from.
	 */
	inline regset output_regs_exact() const {
		regset ret;
		for (unsigned i = 0; i < nregs(); ++i) {
			if (is_reg_output(i)) {
				ret.insert(get_reg(i));
			}
		}
		return ret;
	}

	/**
	 * Get the set of registers that are clobbered by this instruction (i.e. they
	 * are not inputs, but have unknown value after this instruction).
	 */
	inline regset clobbered_regs() const {
		return base->clobbered;
	}

	/**
	 * Insert the set of clobbered registers into the specified regset.
	 *
	 * @param[in,out] rs The regset to insert into.
	 */
	inline void insert_clobbered_regs(regset &rs) const {
		rs.insert_many(base->clobbered);
	}

	/**
	 * Erase the set of clobbered registers from the specified regset.
	 *
	 * @param[in,out] rs The regset to erase from.
	 */
	inline void erase_clobbered_regs(regset &rs) const {
		rs.erase_many(base->clobbered);
	}

	/**
	 * Adjust the set of live variables, passed in as the set of registers live
	 * after this instruction, to reflect the set of registers live before the
	 * instruction.
	 *
	 * @param[in] t        The traits for the architecture of this instruction,
	 *                     used to ignore special registers (e.g. the stack pointer).
	 * @param[in] opts     The options for this function, passed to the arch traits.
	 * @param[in,out] live The regset to be modified.
	 */
	inline void adjust_input_output_set(const arch_traits *t, regset &live) {
		narrow_output_active_mask(live);
		erase_output_regs_wide(live);
		insert_clobbered_regs(live);
		t->adjust_special_regs(live);
		erase_clobbered_regs(live);
		insert_input_regs_exact(live);
	}

	/// Generate a machine readable representation of the current instruction.
	std::string dump() const;

	inline bool operator==(const sloejit::instruction &right) {
		return std::tie(base, regs, literals, targets) ==
		       std::tie(right.base, right.regs, right.literals, right.targets);
	}

	inline bool operator!=(const sloejit::instruction &right) {
		return !(*this == right);
	}
};

} // namespace sloejit

std::ostream &operator<<(std::ostream &os, const sloejit::instruction &i);
