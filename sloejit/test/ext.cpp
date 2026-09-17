/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

static int validate_redundant_ext_elision() {
	sloejit::function fn{ "aarch64_tests_ext_noop", { .validate = true }, aarch64::get_arch_traits() };
	aarch64::instr_builder ib{ fn.make_block("b") };

	ib.make_ext_qqq(aarch64::q0, aarch64::q0, aarch64::q1, 0, aarch64::qv_16b);
	ib.make_ret();

	const auto output_asm = fn.emit_asm();
	return output_asm.find("ext\tv0.16b, v0.16b, v1.16b, #0") == std::string::npos ? 0 : 1;
}

int main() {
	SloejitInstrTest cases("ext");

	cases.add_case(0x2e033841u, "ext\tv1.8b, v2.8b, v3.8b, #7", IB(make_ext_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_8b);
	cases.add_case(0x6e037841u, "ext\tv1.16b, v2.16b, v3.16b, #15", IB(make_ext_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, 15, aarch64::qv_16b);
	cases.add_case(0x05200420u, "ext\tz0.b, z0.b, z1.b, #1", IB(make_ext_zzi), aarch64::z0, aarch64::z1, 1u);
	cases.add_case(0x053f1ce5u, "ext\tz5.b, z5.b, z7.b, #255", IB(make_ext_zzi), aarch64::z5, aarch64::z7,
	               255u);

	return cases.validate() | validate_redundant_ext_elision();
}
