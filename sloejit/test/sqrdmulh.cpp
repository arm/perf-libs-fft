/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sqrdmulh");

	cases.add_case(0x0f43d041u, "sqrdmulh\tv1.4h, v2.4h, v3.h[0]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4h);
	cases.add_case(0x0f53d041u, "sqrdmulh\tv1.4h, v2.4h, v3.h[1]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_4h);
	cases.add_case(0x0f73d841u, "sqrdmulh\tv1.4h, v2.4h, v3.h[7]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 7, aarch64::qv_4h);
	cases.add_case(0x4f43d041u, "sqrdmulh\tv1.8h, v2.8h, v3.h[0]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_8h);
	cases.add_case(0x4f53d041u, "sqrdmulh\tv1.8h, v2.8h, v3.h[1]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_8h);
	cases.add_case(0x4f73d841u, "sqrdmulh\tv1.8h, v2.8h, v3.h[7]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 7, aarch64::qv_8h);
	cases.add_case(0x0f83d041u, "sqrdmulh\tv1.2s, v2.2s, v3.s[0]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_2s);
	cases.add_case(0x0fa3d041u, "sqrdmulh\tv1.2s, v2.2s, v3.s[1]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_2s);
	cases.add_case(0x0fa3d841u, "sqrdmulh\tv1.2s, v2.2s, v3.s[3]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 3, aarch64::qv_2s);
	cases.add_case(0x4f83d041u, "sqrdmulh\tv1.4s, v2.4s, v3.s[0]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4s);
	cases.add_case(0x4fa3d041u, "sqrdmulh\tv1.4s, v2.4s, v3.s[1]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_4s);
	cases.add_case(0x4fa3d841u, "sqrdmulh\tv1.4s, v2.4s, v3.s[3]", IB(make_sqrdmulh_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 3, aarch64::qv_4s);
	cases.add_case(0x2e63b441u, "sqrdmulh\tv1.4h, v2.4h, v3.4h", IB(make_sqrdmulh_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x6e63b441u, "sqrdmulh\tv1.8h, v2.8h, v3.8h", IB(make_sqrdmulh_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x2ea3b441u, "sqrdmulh\tv1.2s, v2.2s, v3.2s", IB(make_sqrdmulh_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x6ea3b441u, "sqrdmulh\tv1.4s, v2.4s, v3.4s", IB(make_sqrdmulh_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x04237441u, "sqrdmulh\tz1.b, z2.b, z3.b", IB(make_sqrdmulh_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04617462u, "sqrdmulh\tz2.h, z3.h, z1.h", IB(make_sqrdmulh_zzz), aarch64::z2, aarch64::z3,
	               aarch64::z1, aarch64::zv_h);
	cases.add_case(0x04a27423u, "sqrdmulh\tz3.s, z1.s, z2.s", IB(make_sqrdmulh_zzz), aarch64::z3, aarch64::z1,
	               aarch64::z2, aarch64::zv_s);
	cases.add_case(0x04e37441u, "sqrdmulh\tz1.d, z2.d, z3.d", IB(make_sqrdmulh_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);
	cases.add_case(0x44bbf441u, "sqrdmulh\tz1.s, z2.s, z3.s[3]", IB(make_sqrdmulh_zzzl), aarch64::z1,
	               aarch64::z2, aarch64::z3, 3, aarch64::zv_s);
	cases.add_case(0x44a1f462u, "sqrdmulh\tz2.s, z3.s, z1.s[0]", IB(make_sqrdmulh_zzzl), aarch64::z2,
	               aarch64::z3, aarch64::z1, 0, aarch64::zv_s);
	cases.add_case(0x44f2f423u, "sqrdmulh\tz3.d, z1.d, z2.d[1]", IB(make_sqrdmulh_zzzl), aarch64::z3,
	               aarch64::z1, aarch64::z2, 1, aarch64::zv_d);
	cases.add_case(0x44e3f441u, "sqrdmulh\tz1.d, z2.d, z3.d[0]", IB(make_sqrdmulh_zzzl), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_d);

	return cases.validate();
}
