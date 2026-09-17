/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("subs");

	cases.add_case(0xeb010043u, "subs\tx3, x2, x1", IB(make_x_subs_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0xeb1f03ffu, "cmp\txzr, xzr", IB(make_x_subs_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0xf1000041u, "subs\tx1, x2, #0", IB(make_x_subs_rri), aarch64::x1, aarch64::x2, 0u);
	cases.add_case(0xf13ffc41u, "subs\tx1, x2, #4095", IB(make_x_subs_rri), aarch64::x1, aarch64::x2, 4095u);
	cases.add_case(0xf17ffc41u, "subs\tx1, x2, #16773120", IB(make_x_subs_rri), aarch64::x1, aarch64::x2,
	               16773120u);
	cases.add_case(0xf10007ffu, "cmp\tsp, #1", IB(make_x_subs_rri), aarch64::xzr, aarch64::sp, 1u);
	cases.add_case(0xeb01005fu, "cmp\tx2, x1", IB(make_x_cmp_rr), aarch64::x2, aarch64::x1);
	cases.add_case(0xeb1f03ffu, "cmp\txzr, xzr", IB(make_x_cmp_rr), aarch64::xzr, aarch64::xzr);

	return cases.validate();
}
