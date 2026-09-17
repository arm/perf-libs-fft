/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld4b");

	cases.add_case(0xa461c000, "ld4b\t{z0.b, z1.b, z2.b, z3.b}, p0/z, [x0, x1]", IB(make_ld4b_zzzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::z3, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa465dfe2, "ld4b\t{z2.b, z3.b, z4.b, z5.b}, p7/z, [sp, x5]", IB(make_ld4b_zzzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::z5, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa460f02b, "ld4b\t{z11.b, z12.b, z13.b, z14.b}, p4/z, [x1]", IB(make_ld4b_zzzzpri),
	               aarch64::z11, aarch64::z12, aarch64::z13, aarch64::z14, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa46bed7e, "ld4b\t{z30.b, z31.b, z0.b, z1.b}, p3/z, [x11, #-20, mul vl]",
	               IB(make_ld4b_zzzzpri), aarch64::z30, aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3,
	               aarch64::x11, -20);

	return cases.validate();
}
