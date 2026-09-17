/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("add");

	cases.add_case(0x8b030041u, "add\tx1, x2, x3", IB(make_x_add_rrr), aarch64::x1, aarch64::x2, aarch64::x3,
	               aarch64::lsl_0);
	cases.add_case(0x8b1f03ffu, "add\txzr, xzr, xzr", IB(make_x_add_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr, aarch64::lsl_0);
	cases.add_case(0x8b03fc41u, "add\tx1, x2, x3, lsl #63", IB(make_x_add_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3, aarch64::lsl_63);
	cases.add_case(0x8b1fffffu, "add\txzr, xzr, xzr, lsl #63", IB(make_x_add_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr, aarch64::lsl_63);
	cases.add_case(0x91000041u, "add\tx1, x2, #0", IB(make_x_add_rri), aarch64::x1, aarch64::x2, 0u);
	cases.add_case(0x913ffc41u, "add\tx1, x2, #4095", IB(make_x_add_rri), aarch64::x1, aarch64::x2, 4095u);
	cases.add_case(0x917ffc41u, "add\tx1, x2, #16773120", IB(make_x_add_rri), aarch64::x1, aarch64::x2,
	               16773120u);
	cases.add_case(0x910007ffu, "add\tsp, sp, #1", IB(make_x_add_rri), aarch64::sp, aarch64::sp, 1u);
	cases.add_case(0x0e238441u, "add\tv1.8b, v2.8b, v3.8b", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8b);
	cases.add_case(0x0e638441u, "add\tv1.4h, v2.4h, v3.4h", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x0ea38441u, "add\tv1.2s, v2.2s, v3.2s", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4e238441u, "add\tv1.16b, v2.16b, v3.16b", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_16b);
	cases.add_case(0x4e638441u, "add\tv1.8h, v2.8h, v3.8h", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x4ea38441u, "add\tv1.4s, v2.4s, v3.4s", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4ee38441u, "add\tv1.2d, v2.2d, v3.2d", IB(make_add_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x2520c001u, "add\tz1.b, z1.b, #0", IB(make_add_zi), aarch64::z1, 0, aarch64::zv_b);
	cases.add_case(0x2520dfe1u, "add\tz1.b, z1.b, #255", IB(make_add_zi), aarch64::z1, 255, aarch64::zv_b);
	cases.add_case(0x2560c001u, "add\tz1.h, z1.h, #0", IB(make_add_zi), aarch64::z1, 0, aarch64::zv_h);
	cases.add_case(0x2560dfe1u, "add\tz1.h, z1.h, #255", IB(make_add_zi), aarch64::z1, 255, aarch64::zv_h);
	cases.add_case(0x2560e021u, "add\tz1.h, z1.h, #256", IB(make_add_zi), aarch64::z1, 256, aarch64::zv_h);
	cases.add_case(0x2560ffe1u, "add\tz1.h, z1.h, #65280", IB(make_add_zi), aarch64::z1, 65280,
	               aarch64::zv_h);
	cases.add_case(0x25a0c001u, "add\tz1.s, z1.s, #0", IB(make_add_zi), aarch64::z1, 0, aarch64::zv_s);
	cases.add_case(0x25a0dfe1u, "add\tz1.s, z1.s, #255", IB(make_add_zi), aarch64::z1, 255, aarch64::zv_s);
	cases.add_case(0x25a0e021u, "add\tz1.s, z1.s, #256", IB(make_add_zi), aarch64::z1, 256, aarch64::zv_s);
	cases.add_case(0x25a0ffe1u, "add\tz1.s, z1.s, #65280", IB(make_add_zi), aarch64::z1, 65280,
	               aarch64::zv_s);
	cases.add_case(0x25e0c001u, "add\tz1.d, z1.d, #0", IB(make_add_zi), aarch64::z1, 0, aarch64::zv_d);
	cases.add_case(0x25e0dfe1u, "add\tz1.d, z1.d, #255", IB(make_add_zi), aarch64::z1, 255, aarch64::zv_d);
	cases.add_case(0x25e0e021u, "add\tz1.d, z1.d, #256", IB(make_add_zi), aarch64::z1, 256, aarch64::zv_d);
	cases.add_case(0x25e0ffe1u, "add\tz1.d, z1.d, #65280", IB(make_add_zi), aarch64::z1, 65280,
	               aarch64::zv_d);
	cases.add_case(0x04230041u, "add\tz1.b, z2.b, z3.b", IB(make_add_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04630041u, "add\tz1.h, z2.h, z3.h", IB(make_add_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x04a30041u, "add\tz1.s, z2.s, z3.s", IB(make_add_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x04e30041u, "add\tz1.d, z2.d, z3.d", IB(make_add_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);
	cases.add_case(0x04000861u, "add\tz1.b, p2/m, z1.b, z3.b", IB(make_add_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04400861u, "add\tz1.h, p2/m, z1.h, z3.h", IB(make_add_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x04800861u, "add\tz1.s, p2/m, z1.s, z3.s", IB(make_add_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x04c00861u, "add\tz1.d, p2/m, z1.d, z3.d", IB(make_add_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
