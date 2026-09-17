/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("sqrshrn2");

	cases.add_case(0x4f0f9c41u, "sqrshrn2\tv1.16b, v2.8h, #1", IB(make_sqrshrn2_qqi), aarch64::q1,
	               aarch64::q2, 1u, aarch64::qv_16b, aarch64::qv_8h);
	cases.add_case(0x4f089c41u, "sqrshrn2\tv1.16b, v2.8h, #8", IB(make_sqrshrn2_qqi), aarch64::q1,
	               aarch64::q2, 8u, aarch64::qv_16b, aarch64::qv_8h);
	cases.add_case(0x4f1f9c41u, "sqrshrn2\tv1.8h, v2.4s, #1", IB(make_sqrshrn2_qqi), aarch64::q1, aarch64::q2,
	               1u, aarch64::qv_8h, aarch64::qv_4s);
	cases.add_case(0x4f109c41u, "sqrshrn2\tv1.8h, v2.4s, #16", IB(make_sqrshrn2_qqi), aarch64::q1,
	               aarch64::q2, 16u, aarch64::qv_8h, aarch64::qv_4s);
	cases.add_case(0x4f3f9c41u, "sqrshrn2\tv1.4s, v2.2d, #1", IB(make_sqrshrn2_qqi), aarch64::q1, aarch64::q2,
	               1u, aarch64::qv_4s, aarch64::qv_2d);
	cases.add_case(0x4f209c41u, "sqrshrn2\tv1.4s, v2.2d, #32", IB(make_sqrshrn2_qqi), aarch64::q1,
	               aarch64::q2, 32u, aarch64::qv_4s, aarch64::qv_2d);

	return cases.validate();
}
