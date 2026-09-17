/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld3q");

	cases.add_case(0xa5218000, "ld3q\t{z0.q, z1.q, z2.q}, p0/z, [x0, x1, lsl #4]", IB(make_ld3q_zzzprr),
	               aarch64::z0, aarch64::z1, aarch64::z2, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xa5259fe2, "ld3q\t{z2.q, z3.q, z4.q}, p7/z, [sp, x5, lsl #4]", IB(make_ld3q_zzzprr),
	               aarch64::z2, aarch64::z3, aarch64::z4, aarch64::p7, aarch64::sp, aarch64::x5);
	cases.add_case(0xa510f02b, "ld3q\t{z11.q, z12.q, z13.q}, p4/z, [x1]", IB(make_ld3q_zzzpri), aarch64::z11,
	               aarch64::z12, aarch64::z13, aarch64::p4, aarch64::x1, 0);
	cases.add_case(0xa514ed7f, "ld3q\t{z31.q, z0.q, z1.q}, p3/z, [x11, #12, mul vl]", IB(make_ld3q_zzzpri),
	               aarch64::z31, aarch64::z0, aarch64::z1, aarch64::p3, aarch64::x11, 12);

	return cases.validate();
}
