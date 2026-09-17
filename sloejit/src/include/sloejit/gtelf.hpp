/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "sloejit_assert.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sloejit {

/**
 * A structure for storing relocation information. Basically identical to the
 * ELF equivalent but with a std::string rather than an offset into the string
 * table.
 */
struct reloc_info {
	std::string name; ///< The target symbol name to lookup.
	int offset; ///< The offset in bytes into the .text section to replace.
	int type; ///< How to do the relocation (see R_AARCH64_* in elf.h).
	int addend; ///< A value to add to the address before relocating, frequently zero.

	reloc_info(decltype(name) name, int offset, int type, int addend)
	    : name(std::move(name)), offset(offset), type(type), addend(addend) {
	}
};

/**
 * A structure for storing miscellaneous notes to aid in debugging or
 * profiling.
 */
struct note_info {
	int64_t ofs;
	std::string note;

	note_info(decltype(ofs) ofs, decltype(note) note) : ofs(ofs), note(std::move(note)) {
	}
};

struct elf_fn_entry {
	std::string name;
	const void *text_addr;
	std::vector<uint8_t> text_bytes;
	const void *data_addr;
	std::vector<uint8_t> data_bytes;
	std::vector<reloc_info> relocs;

	elf_fn_entry() = default;
	elf_fn_entry(const elf_fn_entry &) = default;
	elf_fn_entry(elf_fn_entry &&) = default;
	elf_fn_entry(decltype(name) name, decltype(text_bytes) text_bytes, decltype(data_bytes) data_bytes,
	             decltype(relocs) relocs = {})
	    : name(std::move(name)), text_addr(nullptr), text_bytes(std::move(text_bytes)), data_addr(nullptr),
	      data_bytes(std::move(data_bytes)), relocs(std::move(relocs)) {
	}
	elf_fn_entry(decltype(name) name, decltype(text_addr) text_addr, decltype(text_bytes) text_bytes,
	             decltype(data_addr) data_addr, decltype(data_bytes) data_bytes, decltype(relocs) relocs = {})
	    : name(std::move(name)), text_addr(text_addr), text_bytes(std::move(text_bytes)),
	      data_addr(data_addr), data_bytes(std::move(data_bytes)), relocs(std::move(relocs)) {
	}
};

struct elf_data {
	std::vector<elf_fn_entry> fn_entries;
};

std::vector<uint8_t> emit_elf(const elf_data &);

} // namespace sloejit
