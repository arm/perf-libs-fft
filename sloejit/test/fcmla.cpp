/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fcmla");

	cases.add_case(0x2e43c441u, "fcmla\tv1.4h, v2.4h, v3.4h, #0", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4h);
	cases.add_case(0x2e43cc41u, "fcmla\tv1.4h, v2.4h, v3.4h, #90", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 90, aarch64::qv_4h);
	cases.add_case(0x2e43d441u, "fcmla\tv1.4h, v2.4h, v3.4h, #180", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 180, aarch64::qv_4h);
	cases.add_case(0x2e43dc41u, "fcmla\tv1.4h, v2.4h, v3.4h, #270", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 270, aarch64::qv_4h);
	cases.add_case(0x6e43c441u, "fcmla\tv1.8h, v2.8h, v3.8h, #0", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_8h);
	cases.add_case(0x6e43cc41u, "fcmla\tv1.8h, v2.8h, v3.8h, #90", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 90, aarch64::qv_8h);
	cases.add_case(0x6e43d441u, "fcmla\tv1.8h, v2.8h, v3.8h, #180", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 180, aarch64::qv_8h);
	cases.add_case(0x6e43dc41u, "fcmla\tv1.8h, v2.8h, v3.8h, #270", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 270, aarch64::qv_8h);
	cases.add_case(0x2e83c441u, "fcmla\tv1.2s, v2.2s, v3.2s, #0", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_2s);
	cases.add_case(0x2e83cc41u, "fcmla\tv1.2s, v2.2s, v3.2s, #90", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 90, aarch64::qv_2s);
	cases.add_case(0x2e83d441u, "fcmla\tv1.2s, v2.2s, v3.2s, #180", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 180, aarch64::qv_2s);
	cases.add_case(0x2e83dc41u, "fcmla\tv1.2s, v2.2s, v3.2s, #270", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 270, aarch64::qv_2s);
	cases.add_case(0x6e83c441u, "fcmla\tv1.4s, v2.4s, v3.4s, #0", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4s);
	cases.add_case(0x6e83cc41u, "fcmla\tv1.4s, v2.4s, v3.4s, #90", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 90, aarch64::qv_4s);
	cases.add_case(0x6e83d441u, "fcmla\tv1.4s, v2.4s, v3.4s, #180", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 180, aarch64::qv_4s);
	cases.add_case(0x6e83dc41u, "fcmla\tv1.4s, v2.4s, v3.4s, #270", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 270, aarch64::qv_4s);
	cases.add_case(0x6ec3c441u, "fcmla\tv1.2d, v2.2d, v3.2d, #0", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_2d);
	cases.add_case(0x6ec3cc41u, "fcmla\tv1.2d, v2.2d, v3.2d, #90", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 90, aarch64::qv_2d);
	cases.add_case(0x6ec3d441u, "fcmla\tv1.2d, v2.2d, v3.2d, #180", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 180, aarch64::qv_2d);
	cases.add_case(0x6ec3dc41u, "fcmla\tv1.2d, v2.2d, v3.2d, #270", IB(make_fcmla_qqqi), aarch64::q1,
	               aarch64::q2, aarch64::q3, 270, aarch64::qv_2d);
	cases.add_case(0x64440861u, "fcmla\tz1.h, p2/m, z3.h, z4.h, #0", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 0, aarch64::zv_h);
	cases.add_case(0x64442861u, "fcmla\tz1.h, p2/m, z3.h, z4.h, #90", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 90, aarch64::zv_h);
	cases.add_case(0x64444861u, "fcmla\tz1.h, p2/m, z3.h, z4.h, #180", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 180, aarch64::zv_h);
	cases.add_case(0x64446861u, "fcmla\tz1.h, p2/m, z3.h, z4.h, #270", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 270, aarch64::zv_h);
	cases.add_case(0x64840861u, "fcmla\tz1.s, p2/m, z3.s, z4.s, #0", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 0, aarch64::zv_s);
	cases.add_case(0x64842861u, "fcmla\tz1.s, p2/m, z3.s, z4.s, #90", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 90, aarch64::zv_s);
	cases.add_case(0x64844861u, "fcmla\tz1.s, p2/m, z3.s, z4.s, #180", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 180, aarch64::zv_s);
	cases.add_case(0x64846861u, "fcmla\tz1.s, p2/m, z3.s, z4.s, #270", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 270, aarch64::zv_s);
	cases.add_case(0x64c40861u, "fcmla\tz1.d, p2/m, z3.d, z4.d, #0", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 0, aarch64::zv_d);
	cases.add_case(0x64c42861u, "fcmla\tz1.d, p2/m, z3.d, z4.d, #90", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 90, aarch64::zv_d);
	cases.add_case(0x64c44861u, "fcmla\tz1.d, p2/m, z3.d, z4.d, #180", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 180, aarch64::zv_d);
	cases.add_case(0x64c46861u, "fcmla\tz1.d, p2/m, z3.d, z4.d, #270", IB(make_fcmla_zpzzi), aarch64::z1,
	               aarch64::p2, aarch64::z3, aarch64::z4, 270, aarch64::zv_d);

	return cases.validate();
}
