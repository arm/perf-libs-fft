/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("addvl");

	cases.add_case(0x04225401u, "addvl\tx1, x2, #-32", IB(make_addvl_rri), aarch64::x1, aarch64::x2, -32);
	cases.add_case(0x043f53ffu, "addvl\tsp, sp, #31", IB(make_addvl_rri), aarch64::sp, aarch64::sp, 31);

	return cases.validate();
}
