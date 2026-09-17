/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sqneg");

	cases.add_case(0x2e607841u, "sqneg\tv1.4h, v2.4h", IB(make_sqneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4h);
	cases.add_case(0x6e607841u, "sqneg\tv1.8h, v2.8h", IB(make_sqneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_8h);
	cases.add_case(0x2ea07841u, "sqneg\tv1.2s, v2.2s", IB(make_sqneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2s);
	cases.add_case(0x6ea07841u, "sqneg\tv1.4s, v2.4s", IB(make_sqneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4s);
	cases.add_case(0x6ee07841u, "sqneg\tv1.2d, v2.2d", IB(make_sqneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2d);
	cases.add_case(0x4409a041u, "sqneg\tz1.b, p0/m, z2.b", IB(make_sqneg_zpz), aarch64::z1, aarch64::p0,
	               aarch64::z2, aarch64::zv_b);
	cases.add_case(0x4449a462u, "sqneg\tz2.h, p1/m, z3.h", IB(make_sqneg_zpz), aarch64::z2, aarch64::p1,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x4489a823u, "sqneg\tz3.s, p2/m, z1.s", IB(make_sqneg_zpz), aarch64::z3, aarch64::p2,
	               aarch64::z1, aarch64::zv_s);
	cases.add_case(0x44c9ac41u, "sqneg\tz1.d, p3/m, z2.d", IB(make_sqneg_zpz), aarch64::z1, aarch64::p3,
	               aarch64::z2, aarch64::zv_d);

	return cases.validate();
}
