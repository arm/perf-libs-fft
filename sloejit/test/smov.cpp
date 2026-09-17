/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;

int main() {
	SloejitInstrTest cases("smov");

	cases.add_case(0x4e072c41u, "smov\tx1, v2.b[3]",
	               IBO(make_smov_rql, reg, reg, int, aarch64::q_type_variant), aarch64::x1, aarch64::q2, 3,
	               aarch64::qv_8b);
	cases.add_case(0x4e0f2c83u, "smov\tx3, v4.b[7]",
	               IBO(make_smov_rql, reg, reg, int, aarch64::q_type_variant), aarch64::x3, aarch64::q4, 7,
	               aarch64::qv_16b);
	cases.add_case(0x4e062c25u, "smov\tx5, v1.h[1]",
	               IBO(make_smov_rql, reg, reg, int, aarch64::q_type_variant), aarch64::x5, aarch64::q1, 1,
	               aarch64::qv_4h);
	cases.add_case(0x4e162ce6u, "smov\tx6, v7.h[5]",
	               IBO(make_smov_rql, reg, reg, int, aarch64::q_type_variant), aarch64::x6, aarch64::q7, 5,
	               aarch64::qv_8h);
	cases.add_case(0x4e0c2ca2u, "smov\tx2, v5.s[1]",
	               IBO(make_smov_rql, reg, reg, int, aarch64::q_type_variant), aarch64::x2, aarch64::q5, 1,
	               aarch64::qv_4s);

	return cases.validate();
}
