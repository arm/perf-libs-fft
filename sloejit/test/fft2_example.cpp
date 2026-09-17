/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/aarch64/aarch64.hpp"
#include "sloejit/function.hpp"

#include "write_output.hpp"

namespace aarch64 = sloejit::aarch64;

static void make_fft2_example() {
	sloejit::function fn{ "fft2", {}, aarch64::get_arch_traits() };
	auto init_block = fn.make_block("init");
	auto body_block = fn.make_block("body");
	auto fini_block = fn.make_block("fini");
	aarch64::instr_builder init{ init_block };
	aarch64::instr_builder body{ body_block };
	aarch64::instr_builder fini{ fini_block };
	init.make_cbz_ri(aarch64::x5, fini_block);
	body.make_x_sub_rri(aarch64::x5, aarch64::x5, 1);
	constexpr int lim = 18;
	sloejit::reg x_rs[2 * lim];
	sloejit::reg w_rs[2 * lim];
	for (int i = 0; i < lim; ++i) {
		x_rs[2 * i + 0] =
		    aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(aarch64::x0, 16 * i + 0), aarch64::q_regs);
		x_rs[2 * i + 1] =
		    aarch64::reg_reinterpret_with_class(body.make_d_ldr_ri(aarch64::x0, 16 * i + 8), aarch64::q_regs);
	}
	for (int i = 0; i < lim; ++i) {
		w_rs[2 * i + 0] = aarch64::reg_reinterpret_with_class(
		    body.make_fadd_qq(x_rs[2 * i + 0], x_rs[2 * i + 1], aarch64::qv_2s), aarch64::d_regs);
		w_rs[2 * i + 1] = aarch64::reg_reinterpret_with_class(
		    body.make_fsub_qq(x_rs[2 * i + 0], x_rs[2 * i + 1], aarch64::qv_2s), aarch64::d_regs);
	}
	for (int i = 0; i < lim; ++i) {
		body.make_d_str_rri(w_rs[2 * i + 0], aarch64::x1, 16 * i + 0);
		body.make_d_str_rri(w_rs[2 * i + 1], aarch64::x1, 16 * i + 8);
	}
	body.make_x_add_rri(aarch64::x0, aarch64::x0, 16);
	body.make_x_add_rri(aarch64::x1, aarch64::x1, 16);
	body.make_cbnz_ri(aarch64::x5, body_block);
	fini.make_ret();
#if !defined(__APPLE__) && !defined(_WIN32)
	sloejit::bytevector data = fn.emit_bin();
	sloejit::elf_data e;
	e.fn_entries.emplace_back(fn.name, std::move(data).get(), std::vector<uint8_t>{});
	auto data2 = sloejit::emit_elf(e);

	write_output("aarch64_fft2.o", &data2[0], data2.size());
#endif
}

int main() {
	make_fft2_example();
}
