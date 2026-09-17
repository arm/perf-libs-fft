/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "sloejit_assert.hpp"
#if !defined(__APPLE__) && !defined(_WIN32)
#include <elf.h>
#endif

namespace sloejit {

/**
 * Set members of ELF header appropriately.
 *  @param e_shoff ELF section header offset from start of file in bytes.
 *  @param e_shnum ELF section header table number of entries.
 *  @param e_shstrndx ELF section header string table index.
 */
void fill_elf_eh(Elf64_Ehdr *eh, Elf64_Off e_shoff, Elf64_Half e_shnum, Elf64_Half e_shstrndx);

/**
 * Set members of section header appropriately.
 *  @param sh_name Section name, an offset into the shstrtab.
 *  @param sh_type Section type
 *         (e.g. SHT_NULL, SHT_SYMTAB, SHT_STRTAB, SHT_PROGBITS).
 *  @param sh_flags Section flags, (e.g. SHF_ALLOC, SHF_EXECINSTR).
 *  @param sh_addr Section virtual addr at execution, else 0.
 *  @param sh_offset Section file offset from start of file in bytes.
 *  @param sh_size Section file size in bytes.
 *  @param sh_link Section link to another section
 *         (e.g. SYMTAB sh_link will be index of corresponding STRTAB).
 *  @param sh_info Additional section information
 *         (e.g. SYMTAB sh_info is the number of symbol entries).
 *  @param sh_addralign Section alignment
 *         (only meaningful in object files, where sh_addr would be 0).
 *  @param sh_entsize Entry size if section holds table
 *         (e.g. SYMTAB sh_entsize is sizeof(Elf64_Sym)).
 */
void fill_elf_sh(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Word sh_type, Elf64_Xword sh_flags,
                 Elf64_Addr sh_addr, Elf64_Off sh_offset, Elf64_Xword sh_size, Elf64_Word sh_link,
                 Elf64_Word sh_info, Elf64_Xword sh_addralign, Elf64_Xword sh_entsize);

inline void fill_elf_sh_null(Elf64_Shdr *sh) {
	fill_elf_sh(sh, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);
}

inline void fill_elf_sh_exec_progbits(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Addr sh_addr,
                                      Elf64_Off sh_offset, Elf64_Xword sh_size) {
	Elf64_Xword sh_flags = SHF_ALLOC | SHF_EXECINSTR;
	Elf64_Xword sh_addralign = 4;
	sloejit_assert(sh_addr % sh_addralign == 0);
	fill_elf_sh(sh, sh_name, SHT_PROGBITS, sh_flags, sh_addr, sh_offset, sh_size, 0, 0, sh_addralign, 0);
}

inline void fill_elf_sh_data_progbits(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Addr sh_addr,
                                      Elf64_Off sh_offset, Elf64_Xword sh_size) {
	Elf64_Xword sh_flags = SHF_ALLOC;
	Elf64_Xword sh_addralign = 8;
	sloejit_assert(sh_addr % sh_addralign == 0);
	fill_elf_sh(sh, sh_name, SHT_PROGBITS, sh_flags, sh_addr, sh_offset, sh_size, 0, 0, sh_addralign, 0);
}

/**
 * Set members of a SYMTAB section header appropriately.
 *  @param sh_name Section name, an offset into the shstrtab.
 *  @param sh_offset Section file offset from start of file in bytes.
 *  @param sh_size Section file size in bytes.
 *  @param sh_link Index of corresponding STRTAB.
 *  @param sh_info One greater than the index of the last STB_LOCAL.
 */
inline void fill_elf_sh_symtab(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Off sh_offset, Elf64_Xword sh_size,
                               Elf64_Word sh_link, Elf64_Word sh_info) {
	sloejit_assert(sh_size % sizeof(Elf64_Sym) == 0);
	sloejit_assert(sh_info >= 1);
	Elf64_Xword sh_addralign = 8;
	fill_elf_sh(sh, sh_name, SHT_SYMTAB, 0, 0, sh_offset, sh_size, sh_link, sh_info, sh_addralign,
	            sizeof(Elf64_Sym));
}

/**
 * Set members of a STRTAB section header appropriately.
 *  @param sh_name Section name, an offset into the shstrtab.
 *  @param sh_offset Section file offset from start of file in bytes.
 *  @param sh_size Section file size in bytes.
 */
inline void fill_elf_sh_strtab(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Off sh_offset, Elf64_Xword sh_size) {
	fill_elf_sh(sh, sh_name, SHT_STRTAB, 0, 0, sh_offset, sh_size, 0, 0, 1, 0);
}

/**
 * Set members of a RELA section header appropriately.
 *  @param sh_name Section name, an offset into the shstrtab.
 *  @param sh_offset Section file offset from start of file in bytes.
 *  @param sh_size Section file size in bytes.
 *  @param sh_link Index of corresponding .symtab.
 *  @param sh_info Index of corresponding .text.
 */
inline void fill_elf_sh_rela(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Off sh_offset, Elf64_Xword sh_size,
                             Elf64_Word sh_link, Elf64_Word sh_info) {
	sloejit_assert(sh_size % sizeof(Elf64_Rela) == 0);
	fill_elf_sh(sh, sh_name, SHT_RELA, 0, 0, sh_offset, sh_size, sh_link, sh_info, 8, sizeof(Elf64_Rela));
}

} // namespace sloejit
