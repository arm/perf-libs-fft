/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld3w");

	cases.add_case(0xa541c000, "ld3w\t{z0.s, z1.s, z2.s}, p0/z, [x0, x1, lsl #2]", IB(make_ld3w_zzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa545dfe2, "ld3w\t{z2.s, z3.s, z4.s}, p7/z, [sp, x5, lsl #2]", IB(make_ld3w_zzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa540f02b, "ld3w\t{z11.s, z12.s, z13.s}, p4/z, [x1]", IB(make_ld3w_zzzpri), aarch64::z11,
	               aarch64::z12, aarch64::z13, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa544ed7f, "ld3w\t{z31.s, z0.s, z1.s}, p3/z, [x11, #12, mul vl]", IB(make_ld3w_zzzpri),
	               aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3, aarch64::x11, 12);

	return cases.validate();
}
