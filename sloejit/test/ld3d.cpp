/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld3d");

	cases.add_case(0xa5c1c000, "ld3d\t{z0.d, z1.d, z2.d}, p0/z, [x0, x1, lsl #3]", IB(make_ld3d_zzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa5c5dfe2, "ld3d\t{z2.d, z3.d, z4.d}, p7/z, [sp, x5, lsl #3]", IB(make_ld3d_zzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa5c0f02b, "ld3d\t{z11.d, z12.d, z13.d}, p4/z, [x1]", IB(make_ld3d_zzzpri), aarch64::z11,
	               aarch64::z12, aarch64::z13, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa5c4ed7f, "ld3d\t{z31.d, z0.d, z1.d}, p3/z, [x11, #12, mul vl]", IB(make_ld3d_zzzpri),
	               aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3, aarch64::x11, 12);

	return cases.validate();
}
