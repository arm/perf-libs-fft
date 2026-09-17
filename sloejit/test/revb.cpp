/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("revb");

	cases.add_case(0x05648861u, "revb\tz1.h, p2/m, z3.h", IB(make_revb_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x05a48861u, "revb\tz1.s, p2/m, z3.s", IB(make_revb_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x05e48861u, "revb\tz1.d, p2/m, z3.d", IB(make_revb_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
