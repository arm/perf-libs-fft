/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("st1d");

	cases.add_case(0xe5e8e861u, "st1d\t{z1.d}, p2, [x3, #-8, mul vl]", IB(make_st1d_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xe5e7ebe1u, "st1d\t{z1.d}, p2, [sp, #7, mul vl]", IB(make_st1d_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xe5e44861u, "st1d\t{z1.d}, p2, [x3, x4, lsl #3]", IB(make_st1d_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe5e44be1u, "st1d\t{z1.d}, p2, [sp, x4, lsl #3]", IB(make_st1d_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe584a861u, "st1d\t{z1.d}, p2, [x3, z4.d, lsl #0]", IB(make_st1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe584abe1u, "st1d\t{z1.d}, p2, [sp, z4.d, lsl #0]", IB(make_st1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe5a4a861u, "st1d\t{z1.d}, p2, [x3, z4.d, lsl #3]", IB(make_st1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_3, aarch64::zv_d);
	cases.add_case(0xe5a4abe1u, "st1d\t{z1.d}, p2, [sp, z4.d, lsl #3]", IB(make_st1d_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_3, aarch64::zv_d);
	cases.add_case(0xe0ff4840u, "st1d\t{za0h.d[w14, 0]}, p2, [x2]", IB(make_st1d_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::x2);
	cases.add_case(0xe0ec67e5u, "st1d\t{za2h.d[w15, 1]}, p1, [sp, x12, lsl #3]", IB(make_st1d_alorlprr),
	               aarch64::za, 2, aarch64::hvopt_h, aarch64::x15, 1, aarch64::p1, aarch64::sp, aarch64::x12);
	cases.add_case(0xe0f3f8cfu, "st1d\t{za7v.d[w15, 1]}, p6, [x6, x19, lsl #3]", IB(make_st1d_alorlprr),
	               aarch64::za, 7, aarch64::hvopt_v, aarch64::x15, 1, aarch64::p6, aarch64::x6, aarch64::x19);
	cases.add_case(0xe0ff9062u, "st1d\t{za1v.d[w12, 0]}, p4, [x3]", IB(make_st1d_alorlprr), aarch64::za, 1,
	               aarch64::hvopt_v, aarch64::x12, 0, aarch64::p4, aarch64::x3, aarch64::xzr);

	return cases.validate();
}
