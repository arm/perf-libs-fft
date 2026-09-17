/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sqrdmlah");

	cases.add_case(0x2f43d041u, "sqrdmlah\tv1.4h, v2.4h, v3.h[0]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4h);
	cases.add_case(0x2f53d041u, "sqrdmlah\tv1.4h, v2.4h, v3.h[1]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_4h);
	cases.add_case(0x2f73d841u, "sqrdmlah\tv1.4h, v2.4h, v3.h[7]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 7, aarch64::qv_4h);
	cases.add_case(0x6f43d041u, "sqrdmlah\tv1.8h, v2.8h, v3.h[0]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_8h);
	cases.add_case(0x6f53d041u, "sqrdmlah\tv1.8h, v2.8h, v3.h[1]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_8h);
	cases.add_case(0x6f73d841u, "sqrdmlah\tv1.8h, v2.8h, v3.h[7]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 7, aarch64::qv_8h);
	cases.add_case(0x2f83d041u, "sqrdmlah\tv1.2s, v2.2s, v3.s[0]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_2s);
	cases.add_case(0x2fa3d041u, "sqrdmlah\tv1.2s, v2.2s, v3.s[1]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_2s);
	cases.add_case(0x2fa3d841u, "sqrdmlah\tv1.2s, v2.2s, v3.s[3]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 3, aarch64::qv_2s);
	cases.add_case(0x6f83d041u, "sqrdmlah\tv1.4s, v2.4s, v3.s[0]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4s);
	cases.add_case(0x6fa3d041u, "sqrdmlah\tv1.4s, v2.4s, v3.s[1]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_4s);
	cases.add_case(0x6fa3d841u, "sqrdmlah\tv1.4s, v2.4s, v3.s[3]", IB(make_sqrdmlah_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 3, aarch64::qv_4s);
	cases.add_case(0x2e438441u, "sqrdmlah\tv1.4h, v2.4h, v3.4h", IB(make_sqrdmlah_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x6e438441u, "sqrdmlah\tv1.8h, v2.8h, v3.8h", IB(make_sqrdmlah_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x2e838441u, "sqrdmlah\tv1.2s, v2.2s, v3.2s", IB(make_sqrdmlah_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x6e838441u, "sqrdmlah\tv1.4s, v2.4s, v3.4s", IB(make_sqrdmlah_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x44037041u, "sqrdmlah\tz1.b, z2.b, z3.b", IB(make_sqrdmlah_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x44417062u, "sqrdmlah\tz2.h, z3.h, z1.h", IB(make_sqrdmlah_zzz), aarch64::z2, aarch64::z3,
	               aarch64::z1, aarch64::zv_h);
	cases.add_case(0x44827023u, "sqrdmlah\tz3.s, z1.s, z2.s", IB(make_sqrdmlah_zzz), aarch64::z3, aarch64::z1,
	               aarch64::z2, aarch64::zv_s);
	cases.add_case(0x44c37041u, "sqrdmlah\tz1.d, z2.d, z3.d", IB(make_sqrdmlah_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);
	cases.add_case(0x44a31041u, "sqrdmlah\tz1.s, z2.s, z3.s[0]", IB(make_sqrdmlah_zzzl), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_s);
	cases.add_case(0x44b91062u, "sqrdmlah\tz2.s, z3.s, z1.s[3]", IB(make_sqrdmlah_zzzl), aarch64::z2,
	               aarch64::z3, aarch64::z1, 3, aarch64::zv_s);
	cases.add_case(0x44f21023u, "sqrdmlah\tz3.d, z1.d, z2.d[1]", IB(make_sqrdmlah_zzzl), aarch64::z3,
	               aarch64::z1, aarch64::z2, 1, aarch64::zv_d);
	cases.add_case(0x44e31041u, "sqrdmlah\tz1.d, z2.d, z3.d[0]", IB(make_sqrdmlah_zzzl), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_d);

	return cases.validate();
}
