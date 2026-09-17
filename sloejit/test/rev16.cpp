/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("rev16");

	cases.add_case(0x0e201841u, "rev16\tv1.8b, v2.8b", IB(make_rev16_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_8b);
	cases.add_case(0x4e201841u, "rev16\tv1.16b, v2.16b", IB(make_rev16_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_16b);

	return cases.validate();
}
