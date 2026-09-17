/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld4q");

	cases.add_case(0xa5a18000, "ld4q\t{z0.q, z1.q, z2.q, z3.q}, p0/z, [x0, x1, lsl #4]",
	               IB(make_ld4q_zzzzprr), aarch64::z0, aarch64::z1, aarch64::z2, aarch64::z3, aarch64::p0,
	               aarch64::x0, aarch64::x1);
	cases.add_case(0xa5a59fe2, "ld4q\t{z2.q, z3.q, z4.q, z5.q}, p7/z, [sp, x5, lsl #4]",
	               IB(make_ld4q_zzzzprr), aarch64::z2, aarch64::z3, aarch64::z4, aarch64::z5, aarch64::p7,
	               aarch64::sp, aarch64::x5);
	cases.add_case(0xa590f02b, "ld4q\t{z11.q, z12.q, z13.q, z14.q}, p4/z, [x1]", IB(make_ld4q_zzzzpri),
	               aarch64::z11, aarch64::z12, aarch64::z13, aarch64::z14, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa59bed7e, "ld4q\t{z30.q, z31.q, z0.q, z1.q}, p3/z, [x11, #-20, mul vl]",
	               IB(make_ld4q_zzzzpri), aarch64::z30, aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3,
	               aarch64::x11, -20);

	return cases.validate();
}
