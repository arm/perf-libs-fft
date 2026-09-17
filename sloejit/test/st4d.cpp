/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st4d");

	cases.add_case(0xe5e16000, "st4d\t{z0.d, z1.d, z2.d, z3.d}, p0, [x0, x1, lsl #3]", IB(make_st4d_zzzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::z3, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe5e57fe2, "st4d\t{z2.d, z3.d, z4.d, z5.d}, p7, [sp, x5, lsl #3]", IB(make_st4d_zzzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::z5, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xe5f0f02b, "st4d\t{z11.d, z12.d, z13.d, z14.d}, p4, [x1]", IB(make_st4d_zzzzpri),
	               aarch64::z11, aarch64::z12, aarch64::z13, aarch64::z14, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xe5fbed7e, "st4d\t{z30.d, z31.d, z0.d, z1.d}, p3, [x11, #-20, mul vl]",
	               IB(make_st4d_zzzzpri), aarch64::z30, aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3,
	               aarch64::x11, -20);

	return cases.validate();
}
