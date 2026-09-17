/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("and");

	cases.add_case(0x8a010043u, "and\tx3, x2, x1", IB(make_x_and_rrr), aarch64::x3, aarch64::x2, aarch64::x1);
	cases.add_case(0x8a1f03ffu, "and\txzr, xzr, xzr", IB(make_x_and_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x0e211c43u, "and\tv3.8b, v2.8b, v1.8b", IB(make_and_qqq), aarch64::q3, aarch64::q2,
	               aarch64::q1, aarch64::qv_8b);
	cases.add_case(0x4e211c43u, "and\tv3.16b, v2.16b, v1.16b", IB(make_and_qqq), aarch64::q3, aarch64::q2,
	               aarch64::q1, aarch64::qv_16b);
	cases.add_case(0x04213043u, "and\tz3.d, z2.d, z1.d", IB(make_and_zzz), aarch64::z3, aarch64::z2,
	               aarch64::z1);

	return cases.validate();
}
