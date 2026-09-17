/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("addsvl");

	cases.add_case(0x04225c01u, "addsvl\tx1, x2, #-32", IB(make_addsvl_rri), aarch64::x1, aarch64::x2, -32);
	cases.add_case(0x043f5bffu, "addsvl\tsp, sp, #31", IB(make_addsvl_rri), aarch64::sp, aarch64::sp, 31);
	cases.add_case(0x04205801u, "addsvl\tx1, x0, #0", IB(make_addsvl_rri), aarch64::x1, aarch64::x0, 0);

	return cases.validate();
}
