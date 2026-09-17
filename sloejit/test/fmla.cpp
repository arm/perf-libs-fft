/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fmla");

	cases.add_case(0x0e430c41u, "fmla\tv1.4h, v2.4h, v3.4h", IB(make_fmla_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x4e430c41u, "fmla\tv1.8h, v2.8h, v3.8h", IB(make_fmla_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x0e23cc41u, "fmla\tv1.2s, v2.2s, v3.2s", IB(make_fmla_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4e23cc41u, "fmla\tv1.4s, v2.4s, v3.4s", IB(make_fmla_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4e63cc41u, "fmla\tv1.2d, v2.2d, v3.2d", IB(make_fmla_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x0f031041u, "fmla\tv1.4h, v2.4h, v3.h[0]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4h);
	cases.add_case(0x0f331841u, "fmla\tv1.4h, v2.4h, v3.h[7]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_4h);
	cases.add_case(0x4f031041u, "fmla\tv1.8h, v2.8h, v3.h[0]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_8h);
	cases.add_case(0x4f331841u, "fmla\tv1.8h, v2.8h, v3.h[7]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_8h);
	cases.add_case(0x0f831041u, "fmla\tv1.2s, v2.2s, v3.s[0]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2s);
	cases.add_case(0x0fa31841u, "fmla\tv1.2s, v2.2s, v3.s[3]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_2s);
	cases.add_case(0x4f831041u, "fmla\tv1.4s, v2.4s, v3.s[0]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4s);
	cases.add_case(0x4fa31841u, "fmla\tv1.4s, v2.4s, v3.s[3]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_4s);
	cases.add_case(0x4fc31041u, "fmla\tv1.2d, v2.2d, v3.d[0]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2d);
	cases.add_case(0x4fc31841u, "fmla\tv1.2d, v2.2d, v3.d[1]", IB(make_fmla_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_2d);
	cases.add_case(0x65640861u, "fmla\tz1.h, p2/m, z3.h, z4.h", IB(make_fmla_zpzz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::z4, aarch64::zv_h);
	cases.add_case(0x65a40861u, "fmla\tz1.s, p2/m, z3.s, z4.s", IB(make_fmla_zpzz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::z4, aarch64::zv_s);
	cases.add_case(0x65e40861u, "fmla\tz1.d, p2/m, z3.d, z4.d", IB(make_fmla_zpzz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::z4, aarch64::zv_d);
	cases.add_case(0x64240061u, "fmla\tz1.h, z3.h, z4.h[0]", IB(make_fmla_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 0, aarch64::zv_h);
	cases.add_case(0x647c0061u, "fmla\tz1.h, z3.h, z4.h[7]", IB(make_fmla_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 7, aarch64::zv_h);
	cases.add_case(0x64a40061u, "fmla\tz1.s, z3.s, z4.s[0]", IB(make_fmla_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 0, aarch64::zv_s);
	cases.add_case(0x64bc0061u, "fmla\tz1.s, z3.s, z4.s[3]", IB(make_fmla_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 3, aarch64::zv_s);
	cases.add_case(0x64e40061u, "fmla\tz1.d, z3.d, z4.d[0]", IB(make_fmla_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 0, aarch64::zv_d);
	cases.add_case(0x64f40061u, "fmla\tz1.d, z3.d, z4.d[1]", IB(make_fmla_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 1, aarch64::zv_d);

	return cases.validate();
}
