/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <memory>
#include <string>

namespace sloejit {

struct block;
using block_ptr = std::unique_ptr<block>;
struct function;
using function_ptr = std::unique_ptr<function>;

/**
 * This is just a mostly empty base class for blocks and functions to
 * inherit from. Targets are resolved by pointer address lookup, so
 * the target doesn't actually need to contain anything besides methods
 * to resolve it as a block or a function for CFG purposes.
 */
struct branch_target {
	/// A function-unique id, to be used rather than comparing pointer
	/// addresses (which would cause non-determinism).
	int id;

	/// Functions obviously have names, however for asm-printing and debug
	/// purposes it's easier to just give all branch targets a name!
	std::string name;

	/// @return If this is a block, return it, else assert(false).
	virtual block *as_block() = 0;

	/// @return If this is a function, return it, else assert(false).
	virtual function *as_function() = 0;

protected:
	branch_target(int id, std::string name) : id(id), name(std::move(name)) {
	}
	virtual ~branch_target() = default;
};

} // namespace sloejit
