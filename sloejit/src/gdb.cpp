/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if !defined(__APPLE__) && !defined(_WIN32)
#include "sloejit/gdb.hpp"

#include "gtelf_internal.hpp"
#include "jit_debug_register_code.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum jit_actions_t : uint32_t { JIT_NOACTION = 0, JIT_REGISTER_FN, JIT_UNREGISTER_FN };

struct jit_code_entry {
	struct jit_code_entry *next_entry;
	struct jit_code_entry *prev_entry;
	const char *symfile_addr;
	uint64_t symfile_size;
};

struct jit_descriptor {
	uint32_t version; ///< Must be 1
	uint32_t action_flag; ///< jit_actions_t
	struct jit_code_entry *relevant_entry;
	struct jit_code_entry *first_entry;
};

/* Make sure to specify the version statically, because the
   debugger may check the version before we can set it.  */
struct jit_descriptor __jit_debug_descriptor = { 1, 0, 0, 0 };

Elf64_Ehdr *elf_data;

void sloejit::add_entry(void *sym_addr, int sym_size, const char *sym_name) {
	// .strtab = "\0 sym_name \0"
	//            ^0 ^1       ^ strlen(sym_name) + 1
	size_t name_len = strlen(sym_name);
	size_t strtab_data_len = 1 + name_len + 1;

	const char *shstrtab_data = "\0.symtab\0.shstrtab\0.strtab\0.text\0";
	//                             ^1       ^9         ^19      ^27    ^33
	size_t shstrtab_data_len = 33;

	size_t data_len = strtab_data_len + shstrtab_data_len + 2 * sizeof(Elf64_Sym);

	int shnum = 5;
	size_t elf_data_size = sizeof(Elf64_Ehdr) + shnum * sizeof(Elf64_Shdr) + data_len;
	elf_data = (Elf64_Ehdr *) calloc(1, elf_data_size);
	auto *elf_sh_null = (Elf64_Shdr *) ((char *) elf_data + sizeof(Elf64_Ehdr));
	auto *elf_sh_symtab = (Elf64_Shdr *) ((char *) elf_data + sizeof(Elf64_Ehdr) + sizeof(Elf64_Shdr));
	auto *elf_sh_shstrtab = (Elf64_Shdr *) ((char *) elf_data + sizeof(Elf64_Ehdr) + 2 * sizeof(Elf64_Shdr));
	auto *elf_sh_strtab = (Elf64_Shdr *) ((char *) elf_data + sizeof(Elf64_Ehdr) + 3 * sizeof(Elf64_Shdr));
	auto *elf_sh_text = (Elf64_Shdr *) ((char *) elf_data + sizeof(Elf64_Ehdr) + 4 * sizeof(Elf64_Shdr));
	auto *data_start = (char *) elf_data + sizeof(Elf64_Ehdr) + shnum * sizeof(Elf64_Shdr);

	// fill ELF header
	Elf64_Off e_shoff = sizeof(Elf64_Ehdr);
	Elf64_Half e_shstrndx = 2;
	fill_elf_eh(elf_data, e_shoff, shnum, e_shstrndx);

	// fill each of the `shnum` sections
	fill_elf_sh_null(elf_sh_null);

	auto elf_sh_symtab_off = (data_start - (char *) elf_data) + shstrtab_data_len + strtab_data_len;
	auto elf_sh_symtab_sz = 2 * sizeof(Elf64_Sym);
	auto elf_sh_symtab_link = 3; ///< elf_sh_symtab idx
	auto elf_sh_symtab_info = 2; ///< number of entries
	fill_elf_sh_symtab(elf_sh_symtab, /*sh_name*/ 1, elf_sh_symtab_off, elf_sh_symtab_sz, elf_sh_symtab_link,
	                   elf_sh_symtab_info);

	auto elf_sh_shstrtab_off = data_start - (char *) elf_data;
	auto elf_sh_shstrtab_sz = shstrtab_data_len;
	fill_elf_sh_strtab(elf_sh_shstrtab, /*sh_name*/ 9, elf_sh_shstrtab_off, elf_sh_shstrtab_sz);

	auto elf_sh_strtab_off = (data_start - (char *) elf_data) + shstrtab_data_len;
	auto elf_sh_strtab_sz = strtab_data_len;
	fill_elf_sh_strtab(elf_sh_strtab, /*sh_name*/ 19, elf_sh_strtab_off, elf_sh_strtab_sz);

	auto elf_sh_progbits_addr = (decltype(elf_sh_text->sh_addr)) sym_addr;
	auto elf_sh_progbits_off = 0;
	auto elf_sh_progbits_sz = sym_size;
	fill_elf_sh_exec_progbits(elf_sh_text, /*sh_name*/ 27, elf_sh_progbits_addr, elf_sh_progbits_off,
	                          elf_sh_progbits_sz);

	memcpy((char *) elf_data + elf_sh_shstrtab->sh_offset, shstrtab_data, shstrtab_data_len);
	// Emit string table:
	// .strtab = "\0 sym_name \0"
	//            ^0 ^1       ^ strlen(sym_name) + 1
	char *strtab_dst = (char *) elf_data + elf_sh_strtab->sh_offset;
	strtab_dst[0] = '\0';
	memcpy(strtab_dst + 1, sym_name, name_len);
	strtab_dst[name_len + 1] = '\0';
	auto *sym0 = (Elf64_Sym *) ((char *) elf_data + elf_sh_symtab->sh_offset);
	auto *sym1 = (Elf64_Sym *) ((char *) elf_data + elf_sh_symtab->sh_offset + sizeof(Elf64_Sym));

	sym0->st_name = 0;
	sym0->st_info = 0;
	sym0->st_other = 0;
	sym0->st_shndx = 0;
	sym0->st_value = 0;
	sym0->st_size = 0;

	// st_name is an offset into .strtab. Our symbol starts at offset 1.
	sym1->st_name = 1;
	sym1->st_info = (STB_GLOBAL << 4) | STT_FUNC;
	sym1->st_other = 0;
	sym1->st_shndx = 4; ///< Index of .text
	sym1->st_value = 0; ///< Offset into .text
	sym1->st_size = sym_size;

	// actually allocate entry!
	jit_code_entry *entry = (jit_code_entry *) calloc(1, sizeof(jit_code_entry));
	entry->symfile_addr = (char *) elf_data;
	entry->symfile_size = elf_data_size;

	// set up linked list
	entry->prev_entry = __jit_debug_descriptor.relevant_entry;
	__jit_debug_descriptor.relevant_entry = entry;
	if (entry->prev_entry != NULL) {
		entry->prev_entry->next_entry = entry;
	}
	else {
		__jit_debug_descriptor.first_entry = entry;
	}

	// notify gdb
	__jit_debug_descriptor.action_flag = JIT_REGISTER_FN;
	__jit_debug_register_code();
}
#endif
