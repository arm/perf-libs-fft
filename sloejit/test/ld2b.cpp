/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld2b");

	cases.add_case(0xa421c000, "ld2b\t{z0.b, z1.b}, p0/z, [x0, x1]", IB(make_ld2b_zzprr), aarch64::z0,
	               aarch64::z1, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa425dfe2, "ld2b\t{z2.b, z3.b}, p7/z, [sp, x5]", IB(make_ld2b_zzprr), aarch64::z2,
	               aarch64::z3, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa420f02b, "ld2b\t{z11.b, z12.b}, p4/z, [x1]", IB(make_ld2b_zzpri), aarch64::z11,
	               aarch64::z12, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa423ed68, "ld2b\t{z8.b, z9.b}, p3/z, [x11, #6, mul vl]", IB(make_ld2b_zzpri),
	               aarch64::z8, aarch64::z9, aarch64::p3, aarch64::x11, 6);
	cases.add_case(0xa429ed68, "ld2b\t{z8.b, z9.b}, p3/z, [x11, #-14, mul vl]", IB(make_ld2b_zzpri),
	               aarch64::z8, aarch64::z9, aarch64::p3, aarch64::x11, -14);

	return cases.validate();
}
