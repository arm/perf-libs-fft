/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fcvtl");

	cases.add_case(0x0e217841u, "fcvtl\tv1.4s, v2.4h", IB(make_fcvtl_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0e617841u, "fcvtl\tv1.2d, v2.2s", IB(make_fcvtl_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2d, aarch64::qv_2s);

	return cases.validate();
}
