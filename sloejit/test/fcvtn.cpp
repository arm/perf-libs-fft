/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fcvtn");

	cases.add_case(0x0e216841u, "fcvtn\tv1.4h, v2.4s", IB(make_fcvtn_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4h, aarch64::qv_4s);
	cases.add_case(0x0e616841u, "fcvtn\tv1.2s, v2.2d", IB(make_fcvtn_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2s, aarch64::qv_2d);

	return cases.validate();
}
