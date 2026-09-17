/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fmul");

	cases.add_case(0x1ee30841u, "fmul\th1, h2, h3", IB(make_fmul_hhh), aarch64::h1, aarch64::h2, aarch64::h3);
	cases.add_case(0x1e230841u, "fmul\ts1, s2, s3", IB(make_fmul_sss), aarch64::s1, aarch64::s2, aarch64::s3);
	cases.add_case(0x1e630841u, "fmul\td1, d2, d3", IB(make_fmul_ddd), aarch64::d1, aarch64::d2, aarch64::d3);
	cases.add_case(0x2e431c41u, "fmul\tv1.4h, v2.4h, v3.4h", IB(make_fmul_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x6e431c41u, "fmul\tv1.8h, v2.8h, v3.8h", IB(make_fmul_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x2e23dc41u, "fmul\tv1.2s, v2.2s, v3.2s", IB(make_fmul_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x6e23dc41u, "fmul\tv1.4s, v2.4s, v3.4s", IB(make_fmul_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x6e63dc41u, "fmul\tv1.2d, v2.2d, v3.2d", IB(make_fmul_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x0f039041u, "fmul\tv1.4h, v2.4h, v3.h[0]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4h);
	cases.add_case(0x0f139041u, "fmul\tv1.4h, v2.4h, v3.h[1]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_4h);
	cases.add_case(0x0f339841u, "fmul\tv1.4h, v2.4h, v3.h[7]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_4h);
	cases.add_case(0x4f039041u, "fmul\tv1.8h, v2.8h, v3.h[0]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_8h);
	cases.add_case(0x4f139041u, "fmul\tv1.8h, v2.8h, v3.h[1]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_8h);
	cases.add_case(0x4f339841u, "fmul\tv1.8h, v2.8h, v3.h[7]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_8h);
	cases.add_case(0x0f839041u, "fmul\tv1.2s, v2.2s, v3.s[0]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2s);
	cases.add_case(0x0fa39041u, "fmul\tv1.2s, v2.2s, v3.s[1]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_2s);
	cases.add_case(0x0fa39841u, "fmul\tv1.2s, v2.2s, v3.s[3]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_2s);
	cases.add_case(0x4f839041u, "fmul\tv1.4s, v2.4s, v3.s[0]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4s);
	cases.add_case(0x4fa39041u, "fmul\tv1.4s, v2.4s, v3.s[1]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_4s);
	cases.add_case(0x4fa39841u, "fmul\tv1.4s, v2.4s, v3.s[3]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_4s);
	cases.add_case(0x4fc39041u, "fmul\tv1.2d, v2.2d, v3.d[0]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2d);
	cases.add_case(0x4fc39841u, "fmul\tv1.2d, v2.2d, v3.d[1]", IB(make_fmul_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_2d);
	cases.add_case(0x65430841u, "fmul\tz1.h, z2.h, z3.h", IB(make_fmul_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x65830841u, "fmul\tz1.s, z2.s, z3.s", IB(make_fmul_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x65c30841u, "fmul\tz1.d, z2.d, z3.d", IB(make_fmul_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);
	cases.add_case(0x65428861u, "fmul\tz1.h, p2/m, z1.h, z3.h", IB(make_fmul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x65828861u, "fmul\tz1.s, p2/m, z1.s, z3.s", IB(make_fmul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x65c28861u, "fmul\tz1.d, p2/m, z1.d, z3.d", IB(make_fmul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
