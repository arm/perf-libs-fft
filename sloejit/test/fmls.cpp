/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fmls");

	cases.add_case(0x0ec30c41u, "fmls\tv1.4h, v2.4h, v3.4h", IB(make_fmls_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x4ec30c41u, "fmls\tv1.8h, v2.8h, v3.8h", IB(make_fmls_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x0ea3cc41u, "fmls\tv1.2s, v2.2s, v3.2s", IB(make_fmls_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4ea3cc41u, "fmls\tv1.4s, v2.4s, v3.4s", IB(make_fmls_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4ee3cc41u, "fmls\tv1.2d, v2.2d, v3.2d", IB(make_fmls_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x0f035041u, "fmls\tv1.4h, v2.4h, v3.h[0]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4h);
	cases.add_case(0x0f335841u, "fmls\tv1.4h, v2.4h, v3.h[7]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_4h);
	cases.add_case(0x4f035041u, "fmls\tv1.8h, v2.8h, v3.h[0]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_8h);
	cases.add_case(0x4f335841u, "fmls\tv1.8h, v2.8h, v3.h[7]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_8h);
	cases.add_case(0x0f835041u, "fmls\tv1.2s, v2.2s, v3.s[0]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2s);
	cases.add_case(0x0fa35841u, "fmls\tv1.2s, v2.2s, v3.s[3]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_2s);
	cases.add_case(0x4f835041u, "fmls\tv1.4s, v2.4s, v3.s[0]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4s);
	cases.add_case(0x4fa35841u, "fmls\tv1.4s, v2.4s, v3.s[3]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_4s);
	cases.add_case(0x4fc35041u, "fmls\tv1.2d, v2.2d, v3.d[0]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2d);
	cases.add_case(0x4fc35841u, "fmls\tv1.2d, v2.2d, v3.d[1]", IB(make_fmls_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_2d);
	cases.add_case(0x65642861u, "fmls\tz1.h, p2/m, z3.h, z4.h", IB(make_fmls_zpzz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::z4, aarch64::zv_h);
	cases.add_case(0x65a42861u, "fmls\tz1.s, p2/m, z3.s, z4.s", IB(make_fmls_zpzz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::z4, aarch64::zv_s);
	cases.add_case(0x65e42861u, "fmls\tz1.d, p2/m, z3.d, z4.d", IB(make_fmls_zpzz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::z4, aarch64::zv_d);
	cases.add_case(0x64240461u, "fmls\tz1.h, z3.h, z4.h[0]", IB(make_fmls_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 0, aarch64::zv_h);
	cases.add_case(0x647c0461u, "fmls\tz1.h, z3.h, z4.h[7]", IB(make_fmls_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 7, aarch64::zv_h);
	cases.add_case(0x64a40461u, "fmls\tz1.s, z3.s, z4.s[0]", IB(make_fmls_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 0, aarch64::zv_s);
	cases.add_case(0x64bc0461u, "fmls\tz1.s, z3.s, z4.s[3]", IB(make_fmls_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 3, aarch64::zv_s);
	cases.add_case(0x64e40461u, "fmls\tz1.d, z3.d, z4.d[0]", IB(make_fmls_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 0, aarch64::zv_d);
	cases.add_case(0x64f40461u, "fmls\tz1.d, z3.d, z4.d[1]", IB(make_fmls_zzzl), aarch64::z1, aarch64::z3,
	               aarch64::z4, 1, aarch64::zv_d);

	return cases.validate();
}
