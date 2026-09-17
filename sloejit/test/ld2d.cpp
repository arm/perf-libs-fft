/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld2d");
	cases.add_case(0xa5a1c000, "ld2d\t{z0.d, z1.d}, p0/z, [x0, x1, lsl #3]", IB(make_ld2d_zzprr), aarch64::z0,
	               aarch64::z1, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa5a5dfe2, "ld2d\t{z2.d, z3.d}, p7/z, [sp, x5, lsl #3]", IB(make_ld2d_zzprr), aarch64::z2,
	               aarch64::z3, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa5a0f02b, "ld2d\t{z11.d, z12.d}, p4/z, [x1]", IB(make_ld2d_zzpri), aarch64::z11,
	               aarch64::z12, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa5a3ed68, "ld2d\t{z8.d, z9.d}, p3/z, [x11, #6, mul vl]", IB(make_ld2d_zzpri),
	               aarch64::z8, aarch64::z9, aarch64::p3, aarch64::x11, 6);

	return cases.validate();
}
