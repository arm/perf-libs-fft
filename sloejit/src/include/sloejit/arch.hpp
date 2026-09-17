/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "regset.hpp"
#include "regset_one_space.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace sloejit {

struct block;
struct function;
struct function_options_t;
struct instruction;
struct reg;
struct stack_frame_info;
class live_matrix;
class live_positions;
class regset;

struct arch_traits {
	/// The maximum physical register number used by this architecture.
	const uint64_t max_preg_num;

	/// The set of registers that should be preserved across a function call.
	const regset pcs_preserve;

protected:
	/// Construct an arch_traits structure.
	arch_traits(decltype(max_preg_num) max_preg_num, decltype(pcs_preserve) pcs_preserve)
	    : max_preg_num(max_preg_num), pcs_preserve(std::move(pcs_preserve)) {
	}

public:
	/// Returns true if the specified register id is virtual.
	bool reg_id_is_virtual(uint64_t r_id) const {
		return r_id > max_preg_num;
	}

	/// Returns true if the specified register has a virtual id.
	bool reg_is_virtual(reg r) const {
		return reg_id_is_virtual(r.id);
	}

	/// Get the set of registers for a particular register space.
	virtual const regset_one_space &regs_for_space(uint64_t space_id) const = 0;

	/// Get the set of registers from the lowest half of a particular space
	virtual const regset_one_space &regs_for_space_low_half(uint64_t space_id) const = 0;

	/// Adjust a register set of live registers to reflect registers that are
	/// always/never live. This is used to get the interference set.
	virtual void adjust_special_regs(regset &live) const = 0;

	/// Adjust a register set of live registers to erase registers that are
	/// always/never live. This is used to get spill candidates.
	virtual void erase_special_regs(regset_one_space &live) const = 0;

	/// Emit a spill instruction after the specified instruction, spilling the
	/// specified register to the stack.
	virtual instruction *emit_spill(block *b, instruction *instr, reg r) const = 0;

	/// Emit a load instruction before the specified instruction, loading the
	/// specified register from the stack.
	virtual instruction *emit_reload(block *b, instruction *instr, reg r) const = 0;

	/// Emit a spill instruction after the specified instruction, spilling the
	/// specified register to the stack. This differs from the standard spill
	/// code in that the active_mask of the register can be discarded if only part
	/// of the register needs to be preserved by the PCS.
	virtual instruction *emit_pcs_spill(block *b, instruction *instr, reg r) const = 0;

	/// Emit a load instruction after the specified instruction, loading the
	/// specified register from the stack. This differs from the standard reload
	/// code in that the active_mask of the register can be discarded if only part
	/// of the register needs to be preserved by the PCS.
	virtual instruction *emit_pcs_reload(block *b, instruction *instr, reg r) const = 0;

	/// Get the initial set of register choices for a function, based on what
	/// is input/output to the function and a mapping from vreg -> preg (this is
	/// used for PCS-renamed registers, which have been renamed to allow
	/// register allocation to consider them as valid allocation targets).
	virtual std::map<reg, regset> get_pcs_reg_choices(const std::map<reg, reg> &pcs_vreg_map,
	                                                  const std::vector<reg> &inputs,
	                                                  const std::vector<reg> &outputs) const = 0;

	/// Fixup the offsets present in all spill instructions (passed as
	/// spill_details) if needed. Offsets may not be initialized depending on the
	/// implementation of emit{,_pcs}_{spill,reload} in order to allow for
	/// stack coloring optimizations to reduce stack usage.
	virtual void finalize_spills(function *fn, const stack_frame_info *frame_info,
	                             std::map<reg, std::vector<instruction *>> &spill_details,
	                             const live_matrix &spill_ranges, block *prologue) const = 0;

	/// Fixup any redundant instructions and remove spurious reload instructions if possible.
	virtual void post_regalloc_hook(function *fn) const = 0;

	/// Get a string representation of an instruction opcode.
	virtual std::string opcode_to_string(int opcode) const = 0;
};

} // namespace sloejit
