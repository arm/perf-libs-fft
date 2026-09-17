/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1d");

	cases.add_case(0xa5e8a861u, "ld1d\t{z1.d}, p2/z, [x3, #-8, mul vl]", IB(make_ld1d_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xa5e7abe1u, "ld1d\t{z1.d}, p2/z, [sp, #7, mul vl]", IB(make_ld1d_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xa5e44861u, "ld1d\t{z1.d}, p2/z, [x3, x4, lsl #3]", IB(make_ld1d_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xa5e44be1u, "ld1d\t{z1.d}, p2/z, [sp, x4, lsl #3]", IB(make_ld1d_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xc5c4c861u, "ld1d\t{z1.d}, p2/z, [x3, z4.d, lsl #0]", IB(make_ld1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc5c4cbe1u, "ld1d\t{z1.d}, p2/z, [sp, z4.d, lsl #0]", IB(make_ld1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc5e4c861u, "ld1d\t{z1.d}, p2/z, [x3, z4.d, lsl #3]", IB(make_ld1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_3, aarch64::zv_d);
	cases.add_case(0xc5e4cbe1u, "ld1d\t{z1.d}, p2/z, [sp, z4.d, lsl #3]", IB(make_ld1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_3, aarch64::zv_d);
	cases.add_case(0xe0df0000u, "ld1d\t{za0h.d[w12, 0]}, p0/z, [x0]", IB(make_ld1d_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0);
	cases.add_case(0xe0dff129u, "ld1d\t{za4v.d[w15, 1]}, p4/z, [x9]", IB(make_ld1d_alorlpr), aarch64::za, 4,
	               aarch64::hvopt_v, aarch64::x15, 1, aarch64::p4, aarch64::x9);
	cases.add_case(0xe0c10000u, "ld1d\t{za0h.d[w12, 0]}, p0/z, [x0, x1, lsl #3]", IB(make_ld1d_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe0df6be6u, "ld1d\t{za3h.d[w15, 0]}, p2/z, [sp]", IB(make_ld1d_alorlprr), aarch64::za, 3,
	               aarch64::hvopt_h, aarch64::x15, 0, aarch64::p2, aarch64::sp, aarch64::xzr);
	cases.add_case(0xe0c901a0u, "ld1d\t{za0h.d[w12, 0]}, p0/z, [x13, x9, lsl #3]", IB(make_ld1d_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x13, aarch64::x9);

	return cases.validate();
}
