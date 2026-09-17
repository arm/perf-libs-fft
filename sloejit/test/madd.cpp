/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("madd");

	cases.add_case(0x9b017c43u, "mul\tx3, x2, x1", IB(make_x_mul_rrr), aarch64::x3, aarch64::x2, aarch64::x1);
	cases.add_case(0x9b1f7fffu, "mul\txzr, xzr, xzr", IB(make_x_mul_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x9b020464u, "madd\tx4, x3, x2, x1", IB(make_x_madd_rrrr), aarch64::x4, aarch64::x3,
	               aarch64::x2, aarch64::x1);
	cases.add_case(0x9b1f7fffu, "mul\txzr, xzr, xzr", IB(make_x_madd_rrrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr, aarch64::xzr);

	return cases.validate();
}
