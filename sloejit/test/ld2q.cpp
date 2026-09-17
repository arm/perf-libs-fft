/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld2q");
	cases.add_case(0xa4a18000, "ld2q\t{z0.q, z1.q}, p0/z, [x0, x1, lsl #4]", IB(make_ld2q_zzprr), aarch64::z0,
	               aarch64::z1, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa4a59fe2, "ld2q\t{z2.q, z3.q}, p7/z, [sp, x5, lsl #4]", IB(make_ld2q_zzprr), aarch64::z2,
	               aarch64::z3, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa490f02b, "ld2q\t{z11.q, z12.q}, p4/z, [x1]", IB(make_ld2q_zzpri), aarch64::z11,
	               aarch64::z12, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa493ed68, "ld2q\t{z8.q, z9.q}, p3/z, [x11, #6, mul vl]", IB(make_ld2q_zzpri),
	               aarch64::z8, aarch64::z9, aarch64::p3, aarch64::x11, 6);

	return cases.validate();
}
