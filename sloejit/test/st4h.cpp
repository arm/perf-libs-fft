/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st4h");

	cases.add_case(0xe4e16000, "st4h\t{z0.h, z1.h, z2.h, z3.h}, p0, [x0, x1, lsl #1]", IB(make_st4h_zzzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::z3, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe4e57fe2, "st4h\t{z2.h, z3.h, z4.h, z5.h}, p7, [sp, x5, lsl #1]", IB(make_st4h_zzzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::z5, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xe4f0f02b, "st4h\t{z11.h, z12.h, z13.h, z14.h}, p4, [x1]", IB(make_st4h_zzzzpri),
	               aarch64::z11, aarch64::z12, aarch64::z13, aarch64::z14, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xe4fbed7e, "st4h\t{z30.h, z31.h, z0.h, z1.h}, p3, [x11, #-20, mul vl]",
	               IB(make_st4h_zzzzpri), aarch64::z30, aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3,
	               aarch64::x11, -20);

	return cases.validate();
}
