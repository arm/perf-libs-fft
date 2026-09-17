/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "branch_target.hpp"
#include "instruction.hpp"
#include "reg.hpp"
#include "regmap.hpp"
#include "regset.hpp"
#include "sloejit_assert.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace sloejit {

class bytevector;
struct arch_traits;
struct function_options_t;

/**
 * A basic block of machine instructions for the target architecture.
 * Belongs to a particular function (parent).
 */
struct block : branch_target {
	function *parent; ///< The function owning this basic block.

	/// An unordered list of instructions in this block.
	std::vector<instr_ptr> instrs;

	/// The first instruction in this block, or nullptr if the block is empty.
	instruction *instr_first = nullptr;

	/// The last instruction in this block, or nullptr if the block is empty.
	instruction *instr_last = nullptr;

	/// The set of registers that are inputs to this block.
	regset inputs;

	/// The set of registers that are outputs from this block.
	regset outputs;

	/// The set of blocks that may branch to this block.
	std::set<block *> predecessors;

	/// The set of blocks that may be branched to from this block.
	std::set<block *> successors;

	block(decltype(parent) parent, int id, std::string name)
	    : branch_target(id, std::move(name)), parent(parent) {
	}

	reg fresh_vreg(uint64_t space_id, uint8_t active_mask);
	void update_reg_choices(const arch_traits *t, std::map<reg, regset> &choices);
	bool iterate_input_output_set(const arch_traits *t);
	void substitute_constraint_set(const regmap<reg> &subs);

	void emit_bin(const function_options_t &opts, bytevector &data, int64_t ofs,
	              std::vector<reloc_info> *relocs, std::vector<note_info> *notes);
	std::string emit_asm(const function_options_t &opts, std::vector<note_info> *notes);

	/// Orphan the specified instruction from the current block, and return a unique_ptr to it.
	/// After calling this method, the instruction does not belong to any basic block, and is
	/// hence 'orphaned'. This is useful when the instruction needs to be moved to a new position
	/// in the basic block - it is orphaned first and then inserted at a different position.
	[[nodiscard]] instr_ptr orphan(instruction *instr) {
		sloejit_assert(instr);
		auto it = std::find_if(instrs.begin(), instrs.end(), [instr](auto &x) { return &*x == instr; });
		sloejit_assert(it != instrs.end());
		(instr->instr_prev ? instr->instr_prev->instr_next : instr_first) = instr->instr_next;
		(instr->instr_next ? instr->instr_next->instr_prev : instr_last) = instr->instr_prev;
		instr_ptr ret = std::move(*it);
		instrs.erase(it);
		return ret;
	}

	/// Given an instruction, insert it before the position of another instruction. It
	/// is assumed that the instruction passed in does not belong to any other basic
	/// block, but no checks are performed for this. For safety, using move semantics
	/// to move the instruction handle into this function is recommended.
	void adopt(instr_ptr i, instruction *instr_pos) {
		// update pos s.t.
		// instr_pos->instr_prev->pos  <  i->pos  <  instr_pos->pos
		if (!instr_pos) {
			i->pos = instrs.empty() ? 1 : (instr_last->pos + 1);
		}
		else {
			double pos_a = !instr_pos->instr_prev ? 0 : instr_pos->instr_prev->pos;
			double pos_b = instr_pos->pos;
			i->pos = (pos_a + pos_b) / 2.0;
		}
		// insert i before instr_pos:
		// instr_pos->instr_prev  ==>  i  ==> instr_pos
		i->instr_prev = instr_pos ? instr_pos->instr_prev : instr_last;
		i->instr_next = instr_pos;
		(i->instr_prev ? i->instr_prev->instr_next : instr_first) = i.get();
		(i->instr_next ? i->instr_next->instr_prev : instr_last) = i.get();
		instrs.emplace_back(std::move(i));
	}

	/// Erase the specified instruction completely, removing it from the current block,
	/// and destroying the handle to it.
	void erase(instruction *instr) {
		// orphan returns a unique_ptr, just let it drop and it cleans itself up.
		(void) orphan(instr);
	}

	bool instr_occurs_before(instruction *a, instruction *b) {
		sloejit_assert(a->parent == this);
		sloejit_assert(b->parent == this);
		if (a->pos < b->pos) {
			return true;
		}
		if (a->pos > b->pos) {
			return false;
		}
		while (b && a != b) {
			b = b->instr_next;
		}
		return !b;
	}

	inline block *as_block() override {
		return this;
	}
	inline function *as_function() override {
		sloejit_assert(false);
		return 0;
	}

	/// Generate a machine readable representation of the current block.
	std::string dump() const;
};

} // namespace sloejit

std::ostream &operator<<(std::ostream &os, const sloejit::block &b);
