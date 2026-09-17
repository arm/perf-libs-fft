/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sloejit/aarch64/aarch64.hpp"

#include "write_output.hpp"

namespace aarch64 = sloejit::aarch64;

static void run_long_aarch64_test() {
	sloejit::function fn{ "aarch64_long_tests", {}, aarch64::get_arch_traits() };
	auto b1 = fn.make_block("b1");
	aarch64::instr_builder ib{ b1 };
	constexpr int lim = 30;
	sloejit::reg arr[lim];
	for (int i = 0; i < lim; ++i) {
		arr[i] = ib.make_x_ldrsb_ri(aarch64::x0, i);
	}
	for (int i = 0; i < lim; ++i) {
		ib.make_x_strb_rri(arr[i], aarch64::x1, i);
	}
	ib.make_ret();
	sloejit::bytevector data = fn.emit_bin();

	write_output("aarch64_long.o", &data[0], data.size());
}

int main() {
	run_long_aarch64_test();
}
