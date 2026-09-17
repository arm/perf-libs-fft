/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/aarch64/aarch64.hpp"
#include "sloejit/function.hpp"

#include "write_output.hpp"

static void make_bl_example() {
	sloejit::function caller{ "caller", {}, sloejit::aarch64::get_arch_traits() };
	sloejit::function callee{ "callee", {}, sloejit::aarch64::get_arch_traits() };
	auto caller_block = caller.make_block("init");
	sloejit::aarch64::instr_builder ib{ caller_block };
	ib.make_bl_i(&callee, {}, {}, {});
	ib.make_ret();
#if !defined(__APPLE__) && !defined(_WIN32)
	std::vector<sloejit::reloc_info> relocs;
	sloejit::bytevector data = caller.emit_bin(&relocs);
	sloejit::elf_data e;
	e.fn_entries.emplace_back(caller.name, std::move(data).get(), std::vector<uint8_t>{}, std::move(relocs));
	auto data2 = sloejit::emit_elf(e);

	write_output("aarch64_bl.o", &data2[0], data2.size());
#endif
}

int main() {
	make_bl_example();
}
