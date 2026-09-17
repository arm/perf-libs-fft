/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#if !defined(__APPLE__) && !defined(_WIN32)
#include "gtelf_internal.hpp"

#include "sloejit/gtelf.hpp"

#include <cstring>
#include <iterator>
#include <map>
#include <optional>

// Find the first index where the specified function returns true.
// Returns std::nullopt on failure.
template <typename C, typename F>
auto find_idx(const C &c, F f) -> std::optional<typename C::difference_type> {
	for (auto it = std::begin(c); it != std::end(c); ++it) {
		if (f(*it)) {
			return std::distance(std::begin(c), it);
		}
	}
	return std::nullopt;
}

std::vector<uint8_t> sloejit::emit_elf(const elf_data &e) {
	// layout:
	// elf.0) elf header
	// elf.1a) binary blobs
	// elf.1b) rela data
	// elf.2) shstrtab data
	// elf.3) strtab data
	// elf.4) symtab data
	// elf.5) section headers

	// we just hardcode this, since we don't really need anything else right now.
	const char *shstrtab_base_data = "\0.symtab\0.shstrtab\0.strtab\0";
	//                                  ^1       ^9         ^19      ^27
	size_t shstrtab_base_sz = 27;

	// figure out .symtab symbol order and the total number of symbols
	std::map<std::string, unsigned> symtab_str_to_idx;
	std::map<unsigned, std::string> symtab_idx_to_str;
	std::vector<std::string> strtab_entries;
	// the first symbol is the null value
	unsigned nsyms = 1;
	// the next >=n symbols are the STT_SECTION symbols, one for each .text/.rodata
	for (const auto &fn_e : e.fn_entries) {
		symtab_str_to_idx.emplace(".text." + fn_e.name, nsyms);
		symtab_idx_to_str.emplace(nsyms, ".text." + fn_e.name);
		++nsyms;
		if (!fn_e.data_bytes.empty()) {
			symtab_str_to_idx.emplace(".rodata." + fn_e.name, nsyms);
			symtab_idx_to_str.emplace(nsyms, ".rodata." + fn_e.name);
			++nsyms;
		}
	}
	// the next n symbols are the function names themselves
	for (const auto &fn_e : e.fn_entries) {
		sloejit_assert(symtab_str_to_idx.find(fn_e.name) == symtab_str_to_idx.end());
		symtab_str_to_idx.emplace(fn_e.name, nsyms);
		symtab_idx_to_str.emplace(nsyms, fn_e.name);
		strtab_entries.emplace_back(fn_e.name);
		++nsyms;
	}
	// finally, add any non-local function references in relocations
	for (const auto &fn_e : e.fn_entries) {
		for (auto rela : fn_e.relocs) {
			if (symtab_str_to_idx.find(rela.name) == symtab_str_to_idx.end()) {
				symtab_str_to_idx.emplace(rela.name, nsyms);
				symtab_idx_to_str.emplace(nsyms, rela.name);
				strtab_entries.emplace_back(rela.name);
				++nsyms;
			}
		}
	}

	// calculate size of everything!
	size_t progbits_sz = 0;
	size_t rela_sz = 0;
	size_t rodata_sz = 0;
	size_t shstrtab_sz = shstrtab_base_sz; // elf.2 (see shstrtab_base_data)
	size_t strtab_sz = 1; // elf.3 (null entry at the start)
	std::vector<bool> fn_wants_rela(e.fn_entries.size());
	std::vector<bool> fn_wants_rodata(e.fn_entries.size());
	std::map<int, int> fn_idx_to_sh_text_idx;
	std::map<int, int> fn_idx_to_sh_rela_idx;
	std::map<int, int> fn_idx_to_sh_rodata_idx;
	unsigned shnum = 1; // (null entry at the start)
	for (unsigned i = 0; i < e.fn_entries.size(); ++i) {
		auto &fn_e = e.fn_entries[i];
		shstrtab_sz += fn_e.name.size() + 7; // elf.2 (.text.%s\0)
		progbits_sz += fn_e.text_bytes.size(); // elf.1a
		fn_idx_to_sh_text_idx[i] = shnum++;
		if (!fn_e.relocs.empty()) {
			fn_wants_rela[i] = true;
			fn_idx_to_sh_rela_idx[i] = shnum++;
			shstrtab_sz += fn_e.name.size() + 12; // elf.2 (.rela.text.%s\0)
			rela_sz += fn_e.relocs.size() * sizeof(Elf64_Rela); // elf.1b
		}
		if (!fn_e.data_bytes.empty()) {
			fn_wants_rodata[i] = true;
			fn_idx_to_sh_rodata_idx[i] = shnum++;
			shstrtab_sz += fn_e.name.size() + 9; // elf.2 (.rodata.%s\0)
			rodata_sz += fn_e.data_bytes.size(); // elf.1b
		}
	}
	for (const auto &str : strtab_entries) {
		strtab_sz += str.size() + 1; // elf.3 (including nul-byte)
	}

	// add remaining section headers
	// elf.2) shstrtab data
	// elf.3) strtab data
	// elf.4) symtab data
	shnum += 3;

	size_t ret_size = sizeof(Elf64_Ehdr) + progbits_sz + rela_sz + rodata_sz; // elf.0, elf.1a, elf.1b
	size_t shstrtab_off = ret_size;
	ret_size += shstrtab_sz; // elf.2
	size_t strtab_off = ret_size;
	ret_size += strtab_sz; // elf.3
	size_t symtab_off = ret_size;
	size_t symtab_sz = nsyms * sizeof(Elf64_Sym);
	ret_size += symtab_sz; // elf.4
	size_t shoff = ret_size;
	ret_size += shnum * sizeof(Elf64_Shdr); // elf.5

	// now that we know how big it should be, actually fill it in!
	std::vector<uint8_t> ret(ret_size);

	size_t ofs = 0;
	// emit elf header // elf.0
	fill_elf_eh((Elf64_Ehdr *) &ret[ofs], shoff, shnum, /*shstrndx*/ shnum - 1); // elf.0
	ofs += sizeof(Elf64_Ehdr);

	// emit binary data // elf.1a
	for (const auto &fn_e : e.fn_entries) {
		memcpy((void *) &ret[ofs], (const void *) fn_e.text_bytes.data(), fn_e.text_bytes.size()); // elf.1a
		ofs += fn_e.text_bytes.size();
		memcpy((void *) &ret[ofs], (const void *) fn_e.data_bytes.data(), fn_e.data_bytes.size()); // elf.1a
		ofs += fn_e.data_bytes.size();
	}

	// emit relocation data // elf.1b
	for (const auto &fn_e : e.fn_entries) {
		for (const auto &rela : fn_e.relocs) {
			auto *r = (Elf64_Rela *) &ret[ofs]; // elf.1b
			r->r_offset = rela.offset; // Offset into .text.foo
			r->r_info = ELF64_R_INFO(symtab_str_to_idx.at(rela.name), rela.type); // Index into symtab
			r->r_addend = rela.addend; // Addend (usually 0 for aarch64)
			ofs += sizeof(Elf64_Rela);
		}
	}

	// emit shstrtab data // elf.2
	// shstr.0) base string (includes null, hard-coded section header
	//                       strings like ".symtab", ".symtab")
	// shstr.1a) ".text.foo" for each function "foo"
	// shstr.1b) ".rela.text.foo" for each function "foo", iff this function
	//           contains relocations (e.g. calls another function)

	memcpy((void *) &ret[ofs], shstrtab_base_data, shstrtab_base_sz); // elf.2
	ofs += shstrtab_base_sz;
	for (unsigned i = 0; i < e.fn_entries.size(); ++i) {
		auto &fn_e = e.fn_entries[i];
		// elf.2, shstr.1a ".text.%s"
		memcpy((void *) &ret[ofs], ".text.", 6);
		memcpy((void *) &ret[ofs + 6], fn_e.name.c_str(), fn_e.name.size() + 1);
		ofs += fn_e.name.size() + 7;
		if (fn_wants_rela[i]) {
			// elf.2, shstr.1b ".rela.text.%s"
			memcpy((void *) &ret[ofs], ".rela.text.", 11);
			memcpy((void *) &ret[ofs + 11], fn_e.name.c_str(), fn_e.name.size() + 1);
			ofs += fn_e.name.size() + 12;
		}
		if (fn_wants_rodata[i]) {
			// elf.2, shstr.1b ".rela.text.%s"
			memcpy((void *) &ret[ofs], ".rodata.", 8);
			memcpy((void *) &ret[ofs + 8], fn_e.name.c_str(), fn_e.name.size() + 1);
			ofs += fn_e.name.size() + 9;
		}
	}

	// emit strtab data // elf.3
	// str.0) (null)
	// str.1) symbol names (either function or relocation symbol names)

	std::map<std::string, int> strtab_str_to_ofs;
	ret[ofs++] = 0; // elf.3, str.0 (null entry)
	size_t name_ofs = 1; // (include elf.3 null entry)
	for (const auto &name : strtab_entries) {
		strtab_str_to_ofs[name] = name_ofs;
		memcpy((void *) &ret[ofs], name.c_str(), name.size() + 1); // elf.3, str.1
		ofs += name.size() + 1;
		name_ofs += name.size() + 1;
	}

	// emit symtab entries // elf.4
	// sym.0) (null)
	// sym.1) local entries (STT_SECTION, for each .text.foo, and some for .rodata.foo)
	// sym.2) global entries (functions and relocation names)

	memset((void *) &ret[ofs], 0, sizeof(Elf64_Sym)); // elf.4, sym.0 (null)
	ofs += sizeof(Elf64_Sym);
	unsigned num_stb_local_values = 1;
	for (unsigned fn_i = 0; fn_i < e.fn_entries.size(); ++fn_i) {
		const auto &fn_e = e.fn_entries[fn_i];
		auto sym = (Elf64_Sym *) &ret[ofs]; // elf.4, sym.1
		sym->st_name = 0;
		sym->st_info = (STB_LOCAL << 4) | STT_SECTION;
		sym->st_other = 0;
		sym->st_shndx = fn_idx_to_sh_text_idx.at(fn_i); // Index of .text.foo
		sym->st_value = 0;
		sym->st_size = 0;
		ofs += sizeof(Elf64_Sym);
		++num_stb_local_values;
		if (!fn_e.data_bytes.empty()) {
			auto sym = (Elf64_Sym *) &ret[ofs]; // elf.4, sym.1
			sym->st_name = 0;
			sym->st_info = (STB_LOCAL << 4) | STT_SECTION;
			sym->st_other = 0;
			sym->st_shndx = fn_idx_to_sh_rodata_idx.at(fn_i); // Index of .rodata.foo
			sym->st_value = 0;
			sym->st_size = 0;
			ofs += sizeof(Elf64_Sym);
			++num_stb_local_values;
		}
	}
	for (auto &name : strtab_entries) {
		auto fn_i = find_idx(e.fn_entries, [&name](auto &e) { return e.name == name; });
		auto sym = (Elf64_Sym *) &ret[ofs]; // elf.4, sym.2
		if (!fn_i) {
			// rela entry
			sym->st_name = strtab_str_to_ofs.at(name); // Index into strtab
			sym->st_info = (STB_GLOBAL << 4);
			sym->st_other = 0;
			sym->st_shndx = 0; // Undef
			sym->st_value = 0;
			sym->st_size = 0;
		}
		else {
			// decl entry
			const auto &fn_e = e.fn_entries[*fn_i];
			sym->st_name = strtab_str_to_ofs.at(name); // Index into strtab
			sym->st_info = (STB_GLOBAL << 4) | STT_FUNC;
			sym->st_other = 0;
			sym->st_shndx = fn_idx_to_sh_text_idx.at(*fn_i); // Index of .text.foo
			sym->st_value = 0; // Offset into .text.foo (always 0)
			sym->st_size = fn_e.text_bytes.size();
		}
		ofs += sizeof(Elf64_Sym);
	}

	// section header layout:
	// sh.0) (null)
	// sh.1a) .text.%s  (times n)
	// sh.1b) .rela.text.%s  (up to times n)
	// sh.2) .symtab (including relas)
	// sh.3) .shstrtab
	// sh.4) .strtab

	fill_elf_sh_null((Elf64_Shdr *) &ret[ofs]); // elf.5, sh.0 (null)
	ofs += sizeof(Elf64_Shdr);
	size_t entry_shstrtab_off = shstrtab_base_sz;
	size_t entry_progbits_off = sizeof(Elf64_Ehdr);
	size_t entry_rela_off = sizeof(Elf64_Ehdr) + progbits_sz + rodata_sz;
	for (unsigned i = 0; i < e.fn_entries.size(); ++i) {
		const auto &fn_e = e.fn_entries[i];
		fill_elf_sh_exec_progbits((Elf64_Shdr *) &ret[ofs], entry_shstrtab_off, (Elf64_Addr) fn_e.text_addr,
		                          entry_progbits_off, fn_e.text_bytes.size()); // elf.5, sh.1a (text)
		ofs += sizeof(Elf64_Shdr);
		entry_shstrtab_off += fn_e.name.size() + 7;
		entry_progbits_off += fn_e.text_bytes.size();
		if (fn_wants_rela[i]) {
			auto rela_sz = fn_e.relocs.size() * sizeof(Elf64_Rela);
			auto rela_symtab_link = shnum - 3; // Index of symtab
			auto rela_text_link = fn_idx_to_sh_text_idx.at(i); // Index of .text.foo
			fill_elf_sh_rela((Elf64_Shdr *) &ret[ofs], entry_shstrtab_off, entry_rela_off, rela_sz,
			                 rela_symtab_link, rela_text_link); // elf.5, sh.1b (rela)
			entry_rela_off += rela_sz;
			ofs += sizeof(Elf64_Shdr);
			entry_shstrtab_off += fn_e.name.size() + 12;
		}
		if (fn_wants_rodata[i]) {
			fill_elf_sh_data_progbits((Elf64_Shdr *) &ret[ofs], entry_shstrtab_off,
			                          (Elf64_Addr) fn_e.data_addr, entry_progbits_off,
			                          fn_e.data_bytes.size()); // elf.5, sh.1a (text)
			ofs += sizeof(Elf64_Shdr);
			entry_shstrtab_off += fn_e.name.size() + 9;
			entry_progbits_off += fn_e.data_bytes.size();
		}
	}
	sloejit_assert(entry_shstrtab_off == shstrtab_sz);
	fill_elf_sh_symtab((Elf64_Shdr *) &ret[ofs], 1, symtab_off, symtab_sz, /*.strtab*/ shnum - 2,
	                   num_stb_local_values); // elf.5, sh.2 (symtab)
	ofs += sizeof(Elf64_Shdr);
	fill_elf_sh_strtab((Elf64_Shdr *) &ret[ofs], 19, strtab_off, strtab_sz); // elf.5, sh.3 (strtab)
	ofs += sizeof(Elf64_Shdr);
	fill_elf_sh_strtab((Elf64_Shdr *) &ret[ofs], 9, shstrtab_off, shstrtab_sz); // elf.5, sh.4 (shstrtab)
	ofs += sizeof(Elf64_Shdr);
	sloejit_assert(ofs == ret_size);

	return ret;
}

void sloejit::fill_elf_eh(Elf64_Ehdr *eh, Elf64_Off e_shoff, Elf64_Half e_shnum, Elf64_Half e_shstrndx) {
	// fill e_ident
	eh->e_ident[EI_MAG0] = ELFMAG0;
	eh->e_ident[EI_MAG1] = ELFMAG1;
	eh->e_ident[EI_MAG2] = ELFMAG2;
	eh->e_ident[EI_MAG3] = ELFMAG3;
	eh->e_ident[EI_CLASS] = ELFCLASS64;
	eh->e_ident[EI_DATA] = ELFDATA2LSB;
	eh->e_ident[EI_VERSION] = EV_CURRENT;
	eh->e_ident[EI_OSABI] = ELFOSABI_NONE;
	eh->e_ident[EI_ABIVERSION] = 0;

	// hard-coding to relocatable+aarch64 for now, since that's all we need.
	eh->e_type = ET_REL;
	eh->e_machine = EM_AARCH64;
	eh->e_version = EV_CURRENT;
	eh->e_entry = 0;
	eh->e_phoff = 0;
	eh->e_shoff = e_shoff;
	eh->e_flags = 0;
	eh->e_ehsize = sizeof(Elf64_Ehdr);
	eh->e_phentsize = 0;
	eh->e_phnum = 0;
	eh->e_shentsize = sizeof(Elf64_Shdr);
	eh->e_shnum = e_shnum;
	eh->e_shstrndx = e_shstrndx;
}

void sloejit::fill_elf_sh(Elf64_Shdr *sh, Elf64_Word sh_name, Elf64_Word sh_type, Elf64_Xword sh_flags,
                          Elf64_Addr sh_addr, Elf64_Off sh_offset, Elf64_Xword sh_size, Elf64_Word sh_link,
                          Elf64_Word sh_info, Elf64_Xword sh_addralign, Elf64_Xword sh_entsize) {
	sh->sh_name = sh_name;
	sh->sh_type = sh_type;
	sh->sh_flags = sh_flags;
	sh->sh_addr = sh_addr;
	sh->sh_offset = sh_offset;
	sh->sh_size = sh_size;
	sh->sh_link = sh_link;
	sh->sh_info = sh_info;
	sh->sh_addralign = sh_addralign;
	sh->sh_entsize = sh_entsize;
}
#endif
