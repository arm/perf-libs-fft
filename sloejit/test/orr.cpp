/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("orr");

	cases.add_case(0xaa010043u, "orr\tx3, x2, x1", IB(make_x_orr_rrr), aarch64::x3, aarch64::x2, aarch64::x1);
	cases.add_case(0xaa1f0043u, "orr\tx3, x2, xzr", IB(make_x_orr_rrr), aarch64::x3, aarch64::x2,
	               aarch64::xzr);
	cases.add_case(0xaa0103e3u, "mov\tx3, x1", IB(make_x_orr_rrr), aarch64::x3, aarch64::xzr, aarch64::x1);
	cases.add_case(0xaa01005fu, "orr\txzr, x2, x1", IB(make_x_orr_rrr), aarch64::xzr, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0x0ea11c43u, "orr\tv3.8b, v2.8b, v1.8b", IB(make_orr_qqq), aarch64::q3, aarch64::q2,
	               aarch64::q1, aarch64::qv_8b);
	cases.add_case(0x0ea21c43u, "mov\tv3.8b, v2.8b", IB(make_orr_qqq), aarch64::q3, aarch64::q2, aarch64::q2,
	               aarch64::qv_8b);
	cases.add_case(0x4ea11c43u, "orr\tv3.16b, v2.16b, v1.16b", IB(make_orr_qqq), aarch64::q3, aarch64::q2,
	               aarch64::q1, aarch64::qv_16b);
	cases.add_case(0x4ea21c43u, "mov\tv3.16b, v2.16b", IB(make_orr_qqq), aarch64::q3, aarch64::q2,
	               aarch64::q2, aarch64::qv_16b);
	cases.add_case(0x04613043u, "orr\tz3.d, z2.d, z1.d", IB(make_orr_zzz), aarch64::z3, aarch64::z2,
	               aarch64::z1);
	cases.add_case(0x04623043u, "mov\tz3.d, z2.d", IB(make_orr_zzz), aarch64::z3, aarch64::z2, aarch64::z2);

	return cases.validate();
}
