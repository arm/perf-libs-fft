/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st2w");

	cases.add_case(0xe5216000, "st2w\t{z0.s, z1.s}, p0, [x0, x1, lsl #2]", IB(make_st2w_zzprr), aarch64::z0,
	               aarch64::z1, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe5257fe2, "st2w\t{z2.s, z3.s}, p7, [sp, x5, lsl #2]", IB(make_st2w_zzprr), aarch64::z2,
	               aarch64::z3, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xe530f02b, "st2w\t{z11.s, z12.s}, p4, [x1]", IB(make_st2w_zzpri), aarch64::z11,
	               aarch64::z12, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xe533ed68, "st2w\t{z8.s, z9.s}, p3, [x11, #6, mul vl]", IB(make_st2w_zzpri), aarch64::z8,
	               aarch64::z9, aarch64::p3, aarch64::x11, 6);

	return cases.validate();
}
