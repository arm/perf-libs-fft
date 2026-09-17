/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld3h");

	cases.add_case(0xa4c1c000, "ld3h\t{z0.h, z1.h, z2.h}, p0/z, [x0, x1, lsl #1]", IB(make_ld3h_zzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa4c5dfe2, "ld3h\t{z2.h, z3.h, z4.h}, p7/z, [sp, x5, lsl #1]", IB(make_ld3h_zzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa4c0f02b, "ld3h\t{z11.h, z12.h, z13.h}, p4/z, [x1]", IB(make_ld3h_zzzpri), aarch64::z11,
	               aarch64::z12, aarch64::z13, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa4c4ed7f, "ld3h\t{z31.h, z0.h, z1.h}, p3/z, [x11, #12, mul vl]", IB(make_ld3h_zzzpri),
	               aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3, aarch64::x11, 12);

	return cases.validate();
}
