/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "arch.hpp"
#include "block.hpp"
#include "branch_target.hpp"
#include "bytevector.hpp"
#include "function.hpp"
#include "instruction.hpp"
#include "interval.hpp"
#include "reg.hpp"
#include "regmap.hpp"
#include "sloejit_assert.hpp"
#include "small_vector.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sloejit {

struct live_position_elem {
	instruction *instr = nullptr;
	bool is_input = false;
	bool is_output = false;
	bool is_clobber = false;
};

/**
 * A representation of all occurrences of a particular register.
 * This is useful for building the set of live-ranges later, but note
 * that a live-range may encompass >2 live positions if e.g. a register
 * is both input and output to a particular instruction (either because
 * it is an accumulator or due to encoding constraints).
 * Live positions are tracked over all basic blocks in a program,
 * however this does not include "positions" for variables that are live
 * in-to or out-of a block. Users should refer to a block's input/output
 * regset for this information.
 */
class live_positions {
	// block_id -> ( block*, space_id -> reg -> double (pos) -> elem )
	// TODO: we could just use a regmap here to avoid the vector<map<foo>>
	std::map<int, std::pair<block *, std::vector<std::map<reg, std::map<double, live_position_elem>>>>> elems;

public:
	void emplace(block *b, reg r, instruction *instr, bool is_input, bool is_output, bool is_clobber) {
		auto &[b2, local_elems] = elems[b->id];
		b2 = b;
		if (r.space_id >= local_elems.size()) {
			local_elems.resize(r.space_id + 1);
		}
		auto &elem = local_elems[r.space_id][r][instr->pos];
		sloejit_assert(!elem.instr || elem.instr == instr);
		elem.instr = instr;
		elem.is_input |= is_input;
		elem.is_output |= is_output;
		elem.is_clobber |= is_clobber;
	}

	std::map<double, live_position_elem> &at(block *b, reg r) {
		auto &[b2, local_elems] = elems[b->id];
		b2 = b;
		if (r.space_id >= local_elems.size()) {
			local_elems.resize(r.space_id + 1);
		}
		return local_elems[r.space_id][r];
	}

	template <typename F>
	void map(block *b, reg r, F f) const {
		auto elems_it = elems.find(b->id);
		if (elems_it == elems.end()) return;
		auto &[b2, local_elems] = elems_it->second;
		sloejit_assert(b == b2);
		if (r.space_id >= local_elems.size()) return;
		auto it = local_elems[r.space_id].find(r);
		if (it == local_elems[r.space_id].end()) return;
		// live pos map
		f(it->second);
	}

	template <typename F>
	bool map_until(block *b, F f) const {
		auto elems_it = elems.find(b->id);
		if (elems_it == elems.end()) return false;
		auto &[b2, local_elems] = elems_it->second;
		sloejit_assert(b == b2);
		for (const auto &elems_for_space : local_elems) {
			for (const auto &p : elems_for_space) {
				if (f(p.first, p.second)) {
					return true;
				}
			}
		}
		return false;
	}

	template <typename F>
	void map(block *b, F f) const {
		map_until(b, [&f](reg a, auto &e) {
			f(a, e);
			return false;
		});
	}

	template <typename F>
	void map(reg r, F f) const {
		for (const auto &elem : elems) {
			auto &[b, local_elems] = elem.second;
			if (r.space_id >= local_elems.size()) continue;
			auto &elems_for_space = local_elems[r.space_id];
			auto it = elems_for_space.find(r);
			if (it == elems_for_space.end()) continue;
			// block, live pos map
			f(b, it->second);
		}
	}

	void erase(reg r) {
		for (auto &elem : elems) {
			auto &p = elem.second;
			if (r.space_id >= p.second.size()) continue;
			p.second[r.space_id].erase(r);
		}
	}

	/**
	 * Erase the live position data for occurrences corresponding to the
	 * specified block/instruction (i.e. erase instr->pos from all register
	 * operands of that instruction).
	 *
	 * @param[in] b     The block to erase from. Technically this is redundant
	 *                  information since the instruction knows which block it
	 *                  belongs to, but it is a good consistency check.
	 * @param[in] instr The instruction to erase.
	 */
	void erase(block *b, instruction *instr) {
		auto elems_it = elems.find(b->id);
		if (elems_it == elems.end()) return;
		auto &[b2, space_vec] = elems_it->second;
		sloejit_assert(b == b2);
		for (unsigned i = 0; i < instr->nregs(); ++i) {
			reg r = instr->get_reg(i);
			sloejit_assert(r.space_id < space_vec.size());
			// find the map<double, elem> for this reg
			auto r_it = space_vec[r.space_id].find(r);
			if (r_it == space_vec[r.space_id].end()) continue;
			auto &pos_map = r_it->second;
			pos_map.erase(instr->pos);
			if (pos_map.empty()) {
				// if this was the only occurrence of that pos, we can delete the reg.
				space_vec[r.space_id].erase(r);
			}
		}
	}
};

/**
 * A representation of the live range of a particular virtual register.
 * Note that such a range may be disjoint or span multiple basic-blocks.
 */
class live_range {
	small_vector<interval, 2> elems;

public:
	inline bool overlaps(const live_range &other) const {
		bool ret = false;
		other.elems.map([this, &ret](auto li) {
			elems.map([li, &ret](auto li2) {
				if (intervals_overlap(li, li2)) {
					ret = true;
				}
			});
		});
		return ret;
		;
	}

	inline bool overlaps(interval li) const {
		bool ret = false;
		elems.map([li, &ret](auto li2) {
			if (intervals_overlap(li, li2)) {
				ret = true;
			}
		});
		return ret;
	}

	inline bool empty() const {
		return elems.empty();
	}

	template <typename F>
	void map(F f) const {
		elems.map(f);
	}

	void insert(const live_range &lr) {
		lr.map([this](auto li) { insert(li); });
	}

	void insert(interval li) {
		sloejit_assert(!overlaps(li));
		elems.push_back(li);
	}

	void emplace(double begin, double end) {
		sloejit_assert(!overlaps({ begin, end }));
		elems.emplace_back(begin, end);
	}
};

/**
 * A representation of the live range of several virtual registers
 * for a particular block. The main purpose of this class is to
 * provide a uniform way to add, access and query overlaps for live
 * ranges across an entire function.
 */
class block_live_matrix {
	struct element {
		reg r;
		live_range lr;

		element() = default;
		element(reg r, live_range lr) : r(r), lr(std::move(lr)) {
		}
	};

	static inline bool elem_is_valid(const element &e) {
		return !e.lr.empty();
	}

	/// what block is this live_matrix talking about?
	block *b;

	// reg -> int (index into elems_total[r.space_id])
	regmap<int> elems_lookup;

	// space_id -> [element]
	std::array<std::vector<element>, reg_space_count> elems_total;

	// keep track of which indices in elems_total are unused, so we can reuse
	// them when adding new elems rather than growing the vector if possible.
	std::array<std::vector<int>, reg_space_count> elems_unused;

	/**
	 * Setup a new mapping from the specified register to the specified
	 * live_range, possibly reusing existing (invalid) elems_total entries.
	 *
	 * @param[in] r  The register to index into.
	 * @param[in] lr The live range to add.
	 */
	void add_new_elem(reg r, live_range lr) {
		auto &unused_vec = elems_unused[r.space_id];
		if (unused_vec.empty()) {
			// no unused elems, just throw a new one on the end.
			int idx = elems_total[r.space_id].size();
			elems_total[r.space_id].emplace_back(r, std::move(lr));
			elems_lookup.insert(r, idx);
		}
		else {
			// reuse an existing elems_total idx.
			int idx = unused_vec.back();
			unused_vec.resize(unused_vec.size() - 1);
			elems_total[r.space_id][idx] = element{ r, std::move(lr) };
			elems_lookup.insert(r, idx);
		}
	}

public:
	block_live_matrix(block *b) : b(b) {
	}

	block *get_block() const {
		return b;
	}

	const live_range *at(reg r) const {
		if (elems_lookup.count(r) == 0) return nullptr;
		auto idx = elems_lookup.at(r);
		sloejit_assert(elem_is_valid(elems_total[r.space_id][idx]));
		return &elems_total[r.space_id][idx].lr;
	}

	void emplace(reg r, double begin, double end) {
		sloejit_assert(r.space_id < elems_total.size());
		if (elems_lookup.count(r) == 0) {
			live_range lr;
			lr.emplace(begin, end);
			add_new_elem(r, std::move(lr));
		}
		else {
			int idx = elems_lookup.at(r);
			elems_total[r.space_id][idx].lr.emplace(begin, end);
		}
	}

	void emplace(reg r, interval li) {
		emplace(r, li.begin, li.end);
	}

	void emplace(reg r, live_range lr) {
		sloejit_assert(r.space_id < elems_total.size());
		add_new_elem(r, std::move(lr));
	}

	void erase(reg r) {
		sloejit_assert(r.space_id < elems_total.size());
		if (elems_lookup.count(r)) {
			auto idx = elems_lookup.at(r);
			// mark as invalid but do not delete to avoid
			// needing to re-index the universe!
			elems_total[r.space_id][idx].r = { 0, 0, 0 };
			elems_total[r.space_id][idx].lr = {};
			elems_unused[r.space_id].push_back(idx);
			elems_lookup.erase(r);
		}
	}

	template <typename F>
	void map(F f) const {
		for (unsigned i = 0; i < elems_total.size(); ++i) {
			auto &elems_for_space = elems_total[i];
			for (const auto &e : elems_for_space) {
				if (elem_is_valid(e)) {
					// block, reg, live_range
					f(b, e.r, e.lr);
				}
			}
		}
	}

	template <typename F>
	void map_one_space(uint64_t space_id, F f) const {
		sloejit_assert(space_id < elems_total.size());
		for (const auto &e : elems_total[space_id]) {
			if (elem_is_valid(e)) {
				// reg, live_range
				f(b, e.r, e.lr);
			}
		}
	}

	template <typename F>
	void map_one_space_overlaps(uint64_t space_id, interval li, F f) const {
		sloejit_assert(space_id < elems_total.size());

		// If the function `f` is sufficiently complicated the compiler will not
		// realize that the vector and its size don't change, so do its job for it.
		auto &vec = elems_total[space_id];
		const auto *vec_data = vec.data();
		unsigned vec_sz = vec.size();

		// Most uses of the overlaps function argument (e.g. in constraints setup)
		// is to write into a regset where overlaps occur. By doing the iteration
		// in reverse order, we tend to avoid needing repeated reallocation of the
		// regset internal vectors (since we would tend to simply reallocate once
		// to accommodate the largest reg.id and then work backwards.
		for (unsigned i = vec_sz; i-- > 0;) {
			auto &e = vec_data[i];
			if (elem_is_valid(e) && e.lr.overlaps(li)) {
				// reg
				f(e.r);
			}
		}
	}

	template <typename F>
	void map_one_space_overlaps(uint64_t space_id, const live_range &lr, F f) const {
		sloejit_assert(space_id < elems_total.size());

		// If the function `f` is sufficiently complicated the compiler will not
		// realize that the vector and its size don't change, so do its job for it.
		auto &vec = elems_total[space_id];
		const auto *vec_data = vec.data();
		unsigned vec_sz = vec.size();

		// Most uses of the overlaps function argument (e.g. in constraints setup)
		// is to write into a regset where overlaps occur. By doing the iteration
		// in reverse order, we tend to avoid needing repeated reallocation of the
		// regset internal vectors (since we would tend to simply reallocate once
		// to accommodate the largest reg.id and then work backwards.
		for (unsigned i = vec_sz; i-- > 0;) {
			auto &e = vec_data[i];
			if (elem_is_valid(e) && e.lr.overlaps(lr)) {
				// reg
				f(e.r);
			}
		}
	}
};

/**
 * A representation of the live range of several virtual registers.
 * The main purpose of this class is to provide a uniform way to add,
 * access and query overlaps for live ranges across an entire function.
 */
class live_matrix {
	std::map<int, block_live_matrix> elems;

public:
	std::map<int, std::pair<block *, const live_range *>> at(reg r) const {
		std::map<int, std::pair<block *, const live_range *>> ret;
		for (auto &[b_id, blm] : elems) {
			const auto *elem = blm.at(r);
			if (elem) {
				ret[b_id] = { blm.get_block(), elem };
			}
		}
		sloejit_assert(!ret.empty());
		return ret;
	}

	void emplace(block *b, reg r, double begin, double end) {
		// create the block live matrix if it doesn't already exist
		auto &blm = elems.emplace(b->id, b).first->second;
		blm.emplace(r, begin, end);
	}

	void emplace(block *b, reg r, interval li) {
		// create the block live matrix if it doesn't already exist
		auto &blm = elems.emplace(b->id, b).first->second;
		blm.emplace(r, li);
	}

	void emplace(block *b, reg r, live_range lr) {
		// create the block live matrix if it doesn't already exist
		auto &blm = elems.emplace(b->id, b).first->second;
		blm.emplace(r, std::move(lr));
	}

	void emplace(reg r, std::map<block *, const live_range *> lrs) {
		for (auto [b, lr] : lrs) {
			emplace(b, r, *lr);
		}
	}

	void erase(reg r) {
		for (auto &elem : elems) {
			auto &blm = elem.second;
			blm.erase(r);
		}
	}

	template <typename F>
	void map(F f) const {
		for (const auto &elem : elems) {
			const auto &blm = elem.second;
			blm.map(f);
		}
	}

	template <typename F>
	void map_one_space(uint64_t space_id, F f) const {
		for (const auto &elem : elems) {
			const auto &blm = elem.second;
			blm.map_one_space(space_id, f);
		}
	}

	template <typename F>
	void map_one_space(block *b, uint64_t space_id, F f) const {
		auto it = elems.find(b->id);
		if (it == elems.end()) return;
		it->second.map_one_space(space_id, f);
	}

	template <typename F>
	void map_one_space_overlaps(block *b, uint64_t space_id, interval li, F f) const {
		auto it = elems.find(b->id);
		if (it == elems.end()) return;
		it->second.map_one_space_overlaps(space_id, li, f);
	}

	template <typename F>
	void map_one_space_overlaps(block *b, uint64_t space_id, const live_range &lr, F f) const {
		auto it = elems.find(b->id);
		if (it == elems.end()) return;
		it->second.map_one_space_overlaps(space_id, lr, f);
	}
};

template <typename... Args>
void make_instr(block &b, instruction *instr_pos, const instr_base *base,
                const std::optional<std::string> &tag, Args &&...args) {
	// pos will get filled in by block::adopt.
	auto i = std::make_unique<instruction>(&b, base, 0, tag, std::forward<Args>(args)...);
	b.adopt(std::move(i), instr_pos);
}

} // namespace sloejit
