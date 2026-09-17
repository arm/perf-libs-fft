/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sqrdcmlah");

	cases.add_case(0x44033041u, "sqrdcmlah\tz1.b, z2.b, z3.b, #0", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_b);
	cases.add_case(0x44033441u, "sqrdcmlah\tz1.b, z2.b, z3.b, #90", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 90, aarch64::zv_b);
	cases.add_case(0x44033841u, "sqrdcmlah\tz1.b, z2.b, z3.b, #180", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 180, aarch64::zv_b);
	cases.add_case(0x44033c41u, "sqrdcmlah\tz1.b, z2.b, z3.b, #270", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 270, aarch64::zv_b);
	cases.add_case(0x44433041u, "sqrdcmlah\tz1.h, z2.h, z3.h, #0", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_h);
	cases.add_case(0x44433441u, "sqrdcmlah\tz1.h, z2.h, z3.h, #90", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 90, aarch64::zv_h);
	cases.add_case(0x44433841u, "sqrdcmlah\tz1.h, z2.h, z3.h, #180", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 180, aarch64::zv_h);
	cases.add_case(0x44433c41u, "sqrdcmlah\tz1.h, z2.h, z3.h, #270", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 270, aarch64::zv_h);
	cases.add_case(0x44833041u, "sqrdcmlah\tz1.s, z2.s, z3.s, #0", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_s);
	cases.add_case(0x44833441u, "sqrdcmlah\tz1.s, z2.s, z3.s, #90", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 90, aarch64::zv_s);
	cases.add_case(0x44833841u, "sqrdcmlah\tz1.s, z2.s, z3.s, #180", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 180, aarch64::zv_s);
	cases.add_case(0x44833c41u, "sqrdcmlah\tz1.s, z2.s, z3.s, #270", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 270, aarch64::zv_s);
	cases.add_case(0x44c33041u, "sqrdcmlah\tz1.d, z2.d, z3.d, #0", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 0, aarch64::zv_d);
	cases.add_case(0x44c33441u, "sqrdcmlah\tz1.d, z2.d, z3.d, #90", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 90, aarch64::zv_d);
	cases.add_case(0x44c33841u, "sqrdcmlah\tz1.d, z2.d, z3.d, #180", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 180, aarch64::zv_d);
	cases.add_case(0x44c33c41u, "sqrdcmlah\tz1.d, z2.d, z3.d, #270", IB(make_sqrdcmlah_zzzi), aarch64::z1,
	               aarch64::z2, aarch64::z3, 270, aarch64::zv_d);

	return cases.validate();
}
