/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("sqrshrn");

	cases.add_case(0x0f0f9c41u, "sqrshrn\tv1.8b, v2.8h, #1", IB(make_sqrshrn_qqi), aarch64::q1, aarch64::q2,
	               1u, aarch64::qv_8b, aarch64::qv_8h);
	cases.add_case(0x0f089c41u, "sqrshrn\tv1.8b, v2.8h, #8", IB(make_sqrshrn_qqi), aarch64::q1, aarch64::q2,
	               8u, aarch64::qv_8b, aarch64::qv_8h);
	cases.add_case(0x0f1f9c41u, "sqrshrn\tv1.4h, v2.4s, #1", IB(make_sqrshrn_qqi), aarch64::q1, aarch64::q2,
	               1u, aarch64::qv_4h, aarch64::qv_4s);
	cases.add_case(0x0f109c41u, "sqrshrn\tv1.4h, v2.4s, #16", IB(make_sqrshrn_qqi), aarch64::q1, aarch64::q2,
	               16u, aarch64::qv_4h, aarch64::qv_4s);
	cases.add_case(0x0f3f9c41u, "sqrshrn\tv1.2s, v2.2d, #1", IB(make_sqrshrn_qqi), aarch64::q1, aarch64::q2,
	               1u, aarch64::qv_2s, aarch64::qv_2d);
	cases.add_case(0x0f209c41u, "sqrshrn\tv1.2s, v2.2d, #32", IB(make_sqrshrn_qqi), aarch64::q1, aarch64::q2,
	               32u, aarch64::qv_2s, aarch64::qv_2d);

	return cases.validate();
}
