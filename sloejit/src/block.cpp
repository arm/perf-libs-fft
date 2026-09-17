/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/block.hpp"

#include <sstream>

std::string sloejit::block::dump() const {
	std::ostringstream sstm;
	sstm << *this;
	return std::move(sstm).str();
}

std::ostream &operator<<(std::ostream &os, const sloejit::block &b) {
	os << "{ \"kind\": \"block\", \"children\": [";
	for (auto *instr = b.instr_first; instr; instr = instr->instr_next) {
		os << "\n  " << *instr;
	}
	os << "\n] }";
	return os;
}

static void emit_assert_block_valid(const sloejit::block *b) {
	// Check that the block is non-empty and contains exactly one
	// terminator, which must be at the end of the block.
	sloejit_assert(!b->instrs.empty());
	auto *instr = b->instr_last;
	sloejit_assert(instr->base->kind != sloejit::IK_NORMAL);
	instr = instr->instr_prev;
	for (; instr; instr = instr->instr_prev) {
		sloejit_assert(instr->base->kind == sloejit::IK_NORMAL);
	}
}

void sloejit::block::emit_bin(const function_options_t &opts, sloejit::bytevector &data, int64_t ofs,
                              std::vector<reloc_info> *relocs, std::vector<note_info> *notes) {
	if (opts.validate) {
		emit_assert_block_valid(this);
	}

	for (auto *instr = instr_first; instr; instr = instr->instr_next) {
		instr->base->emit_bin(data, *instr, ofs, relocs);
		if (notes && instr->tag) {
			notes->emplace_back(ofs, *instr->tag);
		}
		ofs += 4; // TODO: this is hardcoded for aarch64
	}
}

std::string sloejit::block::emit_asm(const function_options_t &opts, std::vector<note_info> *notes) {
	if (opts.validate) {
		emit_assert_block_valid(this);
	}

	std::ostringstream sstm;
	for (auto *instr = instr_first; instr; instr = instr->instr_next) {
		sstm << "\t" << instr->base->emit_asm(*instr);
		if (notes && instr->tag) {
			sstm << "\t// " << *instr->tag;
		}
		sstm << "\n";
	}
	return std::move(sstm).str();
}
