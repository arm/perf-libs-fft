/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sub");

	cases.add_case(0xcb010043u, "sub\tx3, x2, x1", IB(make_x_sub_rrr), aarch64::x3, aarch64::x2, aarch64::x1,
	               aarch64::lsl_0);
	cases.add_case(0xcb1f03ffu, "sub\txzr, xzr, xzr", IB(make_x_sub_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr, aarch64::lsl_0);
	cases.add_case(0xcb01fc43u, "sub\tx3, x2, x1, lsl #63", IB(make_x_sub_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1, aarch64::lsl_63);
	cases.add_case(0xcb1fffffu, "sub\txzr, xzr, xzr, lsl #63", IB(make_x_sub_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr, aarch64::lsl_63);
	cases.add_case(0xd1000041u, "sub\tx1, x2, #0", IB(make_x_sub_rri), aarch64::x1, aarch64::x2, 0u);
	cases.add_case(0xd13ffc41u, "sub\tx1, x2, #4095", IB(make_x_sub_rri), aarch64::x1, aarch64::x2, 4095u);
	cases.add_case(0xd17ffc41u, "sub\tx1, x2, #16773120", IB(make_x_sub_rri), aarch64::x1, aarch64::x2,
	               16773120u);
	cases.add_case(0xd10007ffu, "sub\tsp, sp, #1", IB(make_x_sub_rri), aarch64::sp, aarch64::sp, 1u);
	cases.add_case(0x2e238441u, "sub\tv1.8b, v2.8b, v3.8b", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8b);
	cases.add_case(0x2e638441u, "sub\tv1.4h, v2.4h, v3.4h", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x2ea38441u, "sub\tv1.2s, v2.2s, v3.2s", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x6e238441u, "sub\tv1.16b, v2.16b, v3.16b", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_16b);
	cases.add_case(0x6e638441u, "sub\tv1.8h, v2.8h, v3.8h", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x6ea38441u, "sub\tv1.4s, v2.4s, v3.4s", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x6ee38441u, "sub\tv1.2d, v2.2d, v3.2d", IB(make_sub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x2521c001u, "sub\tz1.b, z1.b, #0", IB(make_sub_zi), aarch64::z1, 0, aarch64::zv_b);
	cases.add_case(0x2521dfe1u, "sub\tz1.b, z1.b, #255", IB(make_sub_zi), aarch64::z1, 255, aarch64::zv_b);
	cases.add_case(0x2561c001u, "sub\tz1.h, z1.h, #0", IB(make_sub_zi), aarch64::z1, 0, aarch64::zv_h);
	cases.add_case(0x2561dfe1u, "sub\tz1.h, z1.h, #255", IB(make_sub_zi), aarch64::z1, 255, aarch64::zv_h);
	cases.add_case(0x2561e021u, "sub\tz1.h, z1.h, #256", IB(make_sub_zi), aarch64::z1, 256, aarch64::zv_h);
	cases.add_case(0x2561ffe1u, "sub\tz1.h, z1.h, #65280", IB(make_sub_zi), aarch64::z1, 65280,
	               aarch64::zv_h);
	cases.add_case(0x25a1c001u, "sub\tz1.s, z1.s, #0", IB(make_sub_zi), aarch64::z1, 0, aarch64::zv_s);
	cases.add_case(0x25a1dfe1u, "sub\tz1.s, z1.s, #255", IB(make_sub_zi), aarch64::z1, 255, aarch64::zv_s);
	cases.add_case(0x25a1e021u, "sub\tz1.s, z1.s, #256", IB(make_sub_zi), aarch64::z1, 256, aarch64::zv_s);
	cases.add_case(0x25a1ffe1u, "sub\tz1.s, z1.s, #65280", IB(make_sub_zi), aarch64::z1, 65280,
	               aarch64::zv_s);
	cases.add_case(0x25e1c001u, "sub\tz1.d, z1.d, #0", IB(make_sub_zi), aarch64::z1, 0, aarch64::zv_d);
	cases.add_case(0x25e1dfe1u, "sub\tz1.d, z1.d, #255", IB(make_sub_zi), aarch64::z1, 255, aarch64::zv_d);
	cases.add_case(0x25e1e021u, "sub\tz1.d, z1.d, #256", IB(make_sub_zi), aarch64::z1, 256, aarch64::zv_d);
	cases.add_case(0x25e1ffe1u, "sub\tz1.d, z1.d, #65280", IB(make_sub_zi), aarch64::z1, 65280,
	               aarch64::zv_d);
	cases.add_case(0x04230441u, "sub\tz1.b, z2.b, z3.b", IB(make_sub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04630441u, "sub\tz1.h, z2.h, z3.h", IB(make_sub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x04a30441u, "sub\tz1.s, z2.s, z3.s", IB(make_sub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x04e30441u, "sub\tz1.d, z2.d, z3.d", IB(make_sub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);
	cases.add_case(0x04010861u, "sub\tz1.b, p2/m, z1.b, z3.b", IB(make_sub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04410861u, "sub\tz1.h, p2/m, z1.h, z3.h", IB(make_sub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x04810861u, "sub\tz1.s, p2/m, z1.s, z3.s", IB(make_sub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x04c10861u, "sub\tz1.d, p2/m, z1.d, z3.d", IB(make_sub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
