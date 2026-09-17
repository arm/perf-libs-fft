/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/function.hpp"

#include "sloejit/block.hpp"

#include <sstream>

std::string sloejit::function::dump() const {
	std::ostringstream sstm;
	sstm << *this;
	return std::move(sstm).str();
}

std::ostream &operator<<(std::ostream &os, const sloejit::function &f) {
	os << "{ \"kind\": \"function\", \"name\": \"" << f.name << "\", \"children\": [";
	for (auto &b : f.blocks) {
		os << "\n" << *b;
	}
	os << "\n] }";
	return os;
}

sloejit::bytevector sloejit::function::emit_bin(std::vector<reloc_info> *relocs,
                                                std::vector<note_info> *notes,
                                                const stack_frame_info *frame_info) {
	if (opts.validate) {
		finalize(frame_info);
	}

	// Emit all blocks in order.
	sloejit::bytevector ret;
	int64_t cur_ofs = 0;
	for (auto &b : blocks) {
		b->emit_bin(opts, ret, cur_ofs, relocs, notes);
		// TODO: this is hardcoded for aarch64, which is 4 bytes per instr.
		cur_ofs += b->instrs.size() * 4;
	}
	return ret;
}

std::string sloejit::function::emit_asm(std::vector<note_info> *notes, const stack_frame_info *frame_info) {
	if (opts.validate) {
		finalize(frame_info);
	}

	// Emit all blocks in order.
	std::ostringstream sstm;
#ifndef PLFFT_ENABLE_ARM64EC
	sstm << "\t.global FUNC_NAME(" << name << ")\n";
	sstm << "FUNC_NAME(" << name << "):\n";
#else
	// Defining a MACRO(foo) to print `"#foo"` is (probably) not possible, so do it manually instead
	sstm << "\t.global \"#" << name << "\"\n";
	sstm << "\t.def \"#" << name << "\"\n";
	sstm << "\t.scl 2\n";
	sstm << "\t.type 32\n";
	sstm << "\t.endef\n";
	sstm << "\"#" << name << "\":\n";
#endif
	for (auto &b : blocks) {
		sstm << b->name << ":\n";
		sstm << b->emit_asm(opts, notes);
	}
	return std::move(sstm).str();
}
