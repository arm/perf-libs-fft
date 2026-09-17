/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st2h");

	cases.add_case(0xe4a16000, "st2h\t{z0.h, z1.h}, p0, [x0, x1, lsl #1]", IB(make_st2h_zzprr), aarch64::z0,
	               aarch64::z1, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe4a57fe2, "st2h\t{z2.h, z3.h}, p7, [sp, x5, lsl #1]", IB(make_st2h_zzprr), aarch64::z2,
	               aarch64::z3, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xe4b0f02b, "st2h\t{z11.h, z12.h}, p4, [x1]", IB(make_st2h_zzpri), aarch64::z11,
	               aarch64::z12, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xe4b3ed68, "st2h\t{z8.h, z9.h}, p3, [x11, #6, mul vl]", IB(make_st2h_zzpri), aarch64::z8,
	               aarch64::z9, aarch64::p3, aarch64::x11, 6);

	return cases.validate();
}
