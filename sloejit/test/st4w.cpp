/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st4w");

	cases.add_case(0xe5616000, "st4w\t{z0.s, z1.s, z2.s, z3.s}, p0, [x0, x1, lsl #2]", IB(make_st4w_zzzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::z3, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe5657fe2, "st4w\t{z2.s, z3.s, z4.s, z5.s}, p7, [sp, x5, lsl #2]", IB(make_st4w_zzzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::z5, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xe570f02b, "st4w\t{z11.s, z12.s, z13.s, z14.s}, p4, [x1]", IB(make_st4w_zzzzpri),
	               aarch64::z11, aarch64::z12, aarch64::z13, aarch64::z14, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xe57bed7e, "st4w\t{z30.s, z31.s, z0.s, z1.s}, p3, [x11, #-20, mul vl]",
	               IB(make_st4w_zzzzpri), aarch64::z30, aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3,
	               aarch64::x11, -20);

	return cases.validate();
}
