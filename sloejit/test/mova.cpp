/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("mova");

	cases.add_case(0xc0020000u, "mova\tz0.b, p0/m, za0h.b[w12, 0]", IB(make_mova_zpalorl), aarch64::z0,
	               aarch64::p0, aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, z_type_variant::zv_b);
	cases.add_case(0xc0420000u, "mova\tz0.h, p0/m, za0h.h[w12, 0]", IB(make_mova_zpalorl), aarch64::z0,
	               aarch64::p0, aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, z_type_variant::zv_h);
	cases.add_case(0xc0820000u, "mova\tz0.s, p0/m, za0h.s[w12, 0]", IB(make_mova_zpalorl), aarch64::z0,
	               aarch64::p0, aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, z_type_variant::zv_s);
	cases.add_case(0xc0c20000u, "mova\tz0.d, p0/m, za0h.d[w12, 0]", IB(make_mova_zpalorl), aarch64::z0,
	               aarch64::p0, aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, z_type_variant::zv_d);
	cases.add_case(0xc0c30000u, "mova\tz0.q, p0/m, za0h.q[w12, 0]", IB(make_mova_zpalorl), aarch64::z0,
	               aarch64::p0, aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, z_type_variant::zv_q);

	cases.add_case(0xc0021c60u, "mova\tz0.b, p7/m, za0h.b[w12, 3]", IB(make_mova_zpalorl), aarch64::z0,
	               aarch64::p7, aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 3, z_type_variant::zv_b);
	cases.add_case(0xc042b9a1u, "mova\tz1.h, p6/m, za1v.h[w13, 5]", IB(make_mova_zpalorl), aarch64::z1,
	               aarch64::p6, aarch64::za, 1, aarch64::hvopt_v, aarch64::x13, 5, z_type_variant::zv_h);
	cases.add_case(0xc0825542u, "mova\tz2.s, p5/m, za2h.s[w14, 2]", IB(make_mova_zpalorl), aarch64::z2,
	               aarch64::p5, aarch64::za, 2, aarch64::hvopt_h, aarch64::x14, 2, z_type_variant::zv_s);
	cases.add_case(0xc0c2f163u, "mova\tz3.d, p4/m, za5v.d[w15, 1]", IB(make_mova_zpalorl), aarch64::z3,
	               aarch64::p4, aarch64::za, 5, aarch64::hvopt_v, aarch64::x15, 1, z_type_variant::zv_d);
	cases.add_case(0xc0c30da4u, "mova\tz4.q, p3/m, za13h.q[w12, 0]", IB(make_mova_zpalorl), aarch64::z4,
	               aarch64::p3, aarch64::za, 13, aarch64::hvopt_h, aarch64::x12, 0, z_type_variant::zv_q);

	cases.add_case(0xc0000000, "mova\tza0h.b[w12, 0], p0/m, z0.b", IB(make_mova_alorlpz), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::z0, z_type_variant::zv_b);
	cases.add_case(0xc0400000, "mova\tza0h.h[w12, 0], p0/m, z0.h", IB(make_mova_alorlpz), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::z0, z_type_variant::zv_h);
	cases.add_case(0xc0800000, "mova\tza0h.s[w12, 0], p0/m, z0.s", IB(make_mova_alorlpz), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::z0, z_type_variant::zv_s);
	cases.add_case(0xc0c00000, "mova\tza0h.d[w12, 0], p0/m, z0.d", IB(make_mova_alorlpz), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::z0, z_type_variant::zv_d);
	cases.add_case(0xc0c10000, "mova\tza0h.q[w12, 0], p0/m, z0.q", IB(make_mova_alorlpz), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::z0, z_type_variant::zv_q);

	cases.add_case(0xc000384b, "mova\tza0h.b[w13, 11], p6/m, z2.b", IB(make_mova_alorlpz), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x13, 11, aarch64::p6, aarch64::z2, z_type_variant::zv_b);
	cases.add_case(0xc040f0af, "mova\tza1v.h[w15, 7], p4/m, z5.h", IB(make_mova_alorlpz), aarch64::za, 1,
	               aarch64::hvopt_v, aarch64::x15, 7, aarch64::p4, aarch64::z5, z_type_variant::zv_h);
	cases.add_case(0xc080dd6a, "mova\tza2v.s[w14, 2], p7/m, z11.s", IB(make_mova_alorlpz), aarch64::za, 2,
	               aarch64::hvopt_v, aarch64::x14, 2, aarch64::p7, aarch64::z11, z_type_variant::zv_s);
	cases.add_case(0xc0c0c92d, "mova\tza6v.d[w14, 1], p2/m, z9.d", IB(make_mova_alorlpz), aarch64::za, 6,
	               aarch64::hvopt_v, aarch64::x14, 1, aarch64::p2, aarch64::z9, z_type_variant::zv_d);
	cases.add_case(0xc0c1054b, "mova\tza11h.q[w12, 0], p1/m, z10.q", IB(make_mova_alorlpz), aarch64::za, 11,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p1, aarch64::z10, z_type_variant::zv_q);

	return cases.validate();
}
