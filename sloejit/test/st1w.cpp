/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("st1w");

	cases.add_case(0xe548e861u, "st1w\t{z1.s}, p2, [x3, #-8, mul vl]", IB(make_st1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_s);
	cases.add_case(0xe547ebe1u, "st1w\t{z1.s}, p2, [sp, #7, mul vl]", IB(make_st1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_s);
	cases.add_case(0xe568e861u, "st1w\t{z1.d}, p2, [x3, #-8, mul vl]", IB(make_st1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xe567ebe1u, "st1w\t{z1.d}, p2, [sp, #7, mul vl]", IB(make_st1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xe5444861u, "st1w\t{z1.s}, p2, [x3, x4, lsl #2]", IB(make_st1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xe5444be1u, "st1w\t{z1.s}, p2, [sp, x4, lsl #2]", IB(make_st1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xe5644861u, "st1w\t{z1.d}, p2, [x3, x4, lsl #2]", IB(make_st1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe5644be1u, "st1w\t{z1.d}, p2, [sp, x4, lsl #2]", IB(make_st1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe504a861u, "st1w\t{z1.d}, p2, [x3, z4.d, lsl #0]", IB(make_st1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe504abe1u, "st1w\t{z1.d}, p2, [sp, z4.d, lsl #0]", IB(make_st1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe524a861u, "st1w\t{z1.d}, p2, [x3, z4.d, lsl #2]", IB(make_st1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_2, aarch64::zv_d);
	cases.add_case(0xe524abe1u, "st1w\t{z1.d}, p2, [sp, z4.d, lsl #2]", IB(make_st1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_2, aarch64::zv_d);
	cases.add_case(0xe564c861u, "st1w\t{z1.s}, p2, [x3, z4.s, sxtw #2]", IB(make_st1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_2, aarch64::zv_s);
	cases.add_case(0xe564cbe1u, "st1w\t{z1.s}, p2, [sp, z4.s, sxtw #2]", IB(make_st1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_2, aarch64::zv_s);
	cases.add_case(0xe0bf49a0u, "st1w\t{za0h.s[w14, 0]}, p2, [x13]", IB(make_st1w_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::x13);
	cases.add_case(0xe0a43febu, "st1w\t{za2h.s[w13, 3]}, p7, [sp, x4, lsl #2]", IB(make_st1w_alorlprr),
	               aarch64::za, 2, aarch64::hvopt_h, aarch64::x13, 3, aarch64::p7, aarch64::sp, aarch64::x4);
	cases.add_case(0xe0ab812cu, "st1w\t{za3v.s[w12, 0]}, p0, [x9, x11, lsl #2]", IB(make_st1w_alorlprr),
	               aarch64::za, 3, aarch64::hvopt_v, aarch64::x12, 0, aarch64::p0, aarch64::x9, aarch64::x11);
	cases.add_case(0xe0bf940du, "st1w\t{za3v.s[w12, 1]}, p5, [x0]", IB(make_st1w_alorlprr), aarch64::za, 3,
	               aarch64::hvopt_v, aarch64::x12, 1, aarch64::p5, aarch64::x0, aarch64::xzr);

	return cases.validate();
}
