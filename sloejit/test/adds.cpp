/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("adds");

	cases.add_case(0xab030041u, "adds\tx1, x2, x3", IB(make_x_adds_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0xab1f03ffu, "adds\txzr, xzr, xzr", IB(make_x_adds_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0xb1000041u, "adds\tx1, x2, #0", IB(make_x_adds_rri), aarch64::x1, aarch64::x2, 0u);
	cases.add_case(0xb13ffc41u, "adds\tx1, x2, #4095", IB(make_x_adds_rri), aarch64::x1, aarch64::x2, 4095u);
	cases.add_case(0xb17ffc41u, "adds\tx1, x2, #16773120", IB(make_x_adds_rri), aarch64::x1, aarch64::x2,
	               16773120u);
	cases.add_case(0xb10007ffu, "adds\txzr, sp, #1", IB(make_x_adds_rri), aarch64::xzr, aarch64::sp, 1u);

	return cases.validate();
}
