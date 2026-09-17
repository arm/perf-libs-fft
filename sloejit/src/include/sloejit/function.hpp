/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "block.hpp"
#include "branch_target.hpp"
#include "function_options.hpp"
#include "gen.hpp"
#include "gtelf.hpp"
#include "reg.hpp"
#include "sloejit_assert.hpp"

#include <sstream>
#include <string>

namespace sloejit {

struct arch_traits;

/**
 * A placeholder to represent any rodata-section references.
 * Belongs to a particular function (parent).
 */
struct rodata_section : branch_target {
	function *parent; ///< The function owning this basic block.

	rodata_section(function *parent, int id, std::string name)
	    : branch_target(id, std::move(name)), parent(parent) {
	}

	inline block *as_block() override {
		sloejit_assert(false);
		return 0;
	}
	inline function *as_function() override {
		sloejit_assert(false);
		return 0;
	}
};

/**
 * Input information about stack usage. Needed so that if register
 * allocation messes with the stack we can at least try to preserve the
 * original intent of pre-existing loads and stores to the stack.
 */
struct stack_frame_info {
	int parent_call_arg_stack_size = 0;
	int child_call_arg_stack_size = 0;
};

/**
 * A function encapsulating basic blocks into a single unit.
 * Owns all basic blocks under it and also controls input/output parameters
 * and calling-convention details.
 */
struct function : branch_target {
	/// The set of options to use when doing finalize/emit for this function.
	function_options_t opts;

	/// The architecture used in this function.
	const arch_traits *traits;

	/// The fresh-register generator for this function, one for each register space.
	std::array<generator, 4> gen;

	/// An ordered list of blocks in this function. The entry point is the first
	/// block and blocks will be emitted in this order.
	std::vector<block_ptr> blocks;

	/// The set of registers that are inputs to this function.
	std::vector<reg> inputs;

	/// The set of registers that are outputs from this function.
	std::vector<reg> outputs;

	/// The read-only data section of this function, used primarily as a branch target.
	rodata_section rodata;

	/// Have we called finalize yet? Avoid doing that logic twice!
	bool finalized = false;

	function(std::string name, function_options_t opts, const arch_traits *traits)
	    : branch_target(0, std::move(name)), opts(opts), traits(traits),
	      rodata(this, 1, ".rodata." + this->name) {
	}

	/// Perform register/target allocation, called from emit_xxxx.
	void finalize(const stack_frame_info *frame_info);

	/// Write this function out to a bytevector, filling in any relocations.
	bytevector emit_bin(std::vector<reloc_info> *relocs = nullptr, std::vector<note_info> *notes = nullptr,
	                    const stack_frame_info *frame_info = nullptr);

	/// Write this function out to a string.
	std::string emit_asm(std::vector<note_info> *notes = nullptr,
	                     const stack_frame_info *frame_info = nullptr);

	/// Get a fresh virtual register with the specified space/class.
	reg fresh_vreg(uint64_t space_id, uint8_t active_mask);

	inline block *make_block(std::string name) {
		return make_block(name, blocks.size());
	}

	inline block *make_block(std::string name, unsigned ofs) {
		std::ostringstream sstm;
		sstm << ".L" << this->name << "." << name;
		std::string global_name = sstm.str();

		sloejit_assert(ofs <= blocks.size());
		blocks.emplace(blocks.begin() + ofs,
		               std::make_unique<block>(this, blocks.size() + 2, std::move(global_name)));
		return &*blocks[ofs];
	}

	inline block *as_block() override {
		sloejit_assert(false);
		return 0;
	}
	inline function *as_function() override {
		return this;
	}

	/// Generate a machine readable representation of the current block.
	std::string dump() const;
};

} // namespace sloejit

std::ostream &operator<<(std::ostream &os, const sloejit::function &f);
