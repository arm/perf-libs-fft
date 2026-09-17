/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/instruction.hpp"

#include "sloejit/arch.hpp"
#include "sloejit/block.hpp"
#include "sloejit/function.hpp"

#include <sstream>

std::string sloejit::instruction::dump() const {
	std::ostringstream sstm;
	sstm << *this;
	return std::move(sstm).str();
}

std::ostream &operator<<(std::ostream &os, const sloejit::instruction &instr) {
	auto *arch = instr.parent->parent->traits;
	os << "{ \"kind\": \"instruction\", ";
	os << "\"opcode\": \"" << arch->opcode_to_string(instr.base->opcode) << "\", ";
	os << "\"args\": [";
	for (unsigned i = 0; i < instr.nregs(); ++i) {
		if (i) os << ", ";
		os << instr.get_reg(i).id;
	}
	os << "], \"imm\": [";
	for (unsigned i = 0; i < instr.literals.size(); ++i) {
		if (i) os << ", ";
		os << instr.literals[i];
	}
	os << "] }";
	return os;
}
