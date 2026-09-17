/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "interval.hpp"
#include "reg.hpp"
#include "regset.hpp"

namespace sloejit {

struct block;
struct instruction;

enum class live_range_edit_kind { insert_pos, erase_instr, insert_interval, apply_reg_choices, erase_reg };

/**
 * A representation of an individual edit that should be made to the
 * live range set. This is not done directly since we may need to also
 * update auxiliary data structures like constraint sets.
 */
struct live_range_edit {
	live_range_edit_kind kind;
	block *b; ///< What block are we talking about.
	reg r; ///< What register is this edit operating on.
	interval li; ///< The interval to insert, if present.
	regset rc; ///< The reg choices to apply, if present.
	instruction *instr; ///< If inserting a position, which instruction?
	bool is_input; ///< If inserting a position, are we an input?
	bool is_output; ///< If inserting a position, are we an output?
	bool is_clobber; ///< If inserting a position, are we a clobber?

	live_range_edit() = default;
	live_range_edit(const live_range_edit &) = default;
	live_range_edit(live_range_edit &&) = default;
	live_range_edit(live_range_edit_kind kind, block *b, reg r, interval li, regset rc, instruction *instr,
	                bool is_input, bool is_output, bool is_clobber)
	    : kind(kind), b(b), r(r), li(std::move(li)), rc(std::move(rc)), instr(instr), is_input(is_input),
	      is_output(is_output), is_clobber(is_clobber) {
	}
};

/// Make a live range edit that inserts the specified interval
static inline live_range_edit make_lr_edit_insert_interval(block *b, reg r, interval li) {
	return { live_range_edit_kind::insert_interval, b, r, std::move(li), {}, 0, false, false, false };
}

/// Make a live range edit that updates the reg choices for this register
static inline live_range_edit make_lr_edit_apply_reg_choices(reg r, regset rc) {
	return { live_range_edit_kind::apply_reg_choices, nullptr, r, {}, std::move(rc), 0, false, false, false };
}

/// Make a live range edit that completely erases the specified register
static inline live_range_edit make_lr_edit_erase_reg(reg r) {
	return { live_range_edit_kind::erase_reg, nullptr, r, {}, {}, 0, false, false, false };
}

/// Make a live range edit that inserts the specified position
static inline live_range_edit make_lr_edit_insert_pos(block *b, reg r, instruction *instr, bool is_input,
                                                      bool is_output, bool is_clobber) {
	return { live_range_edit_kind::insert_pos, b, r, {}, {}, instr, is_input, is_output, is_clobber };
}

/// Make a live range edit that erases the specified instruction
static inline live_range_edit make_lr_edit_erase_instr(block *b, instruction *instr) {
	return { live_range_edit_kind::erase_instr, b, {}, {}, {}, instr, false, false, false };
}

} // namespace sloejit
