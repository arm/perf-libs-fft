/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("st1h");

	cases.add_case(0xe4a8e861u, "st1h\t{z1.h}, p2, [x3, #-8, mul vl]", IB(make_st1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_h);
	cases.add_case(0xe4a7ebe1u, "st1h\t{z1.h}, p2, [sp, #7, mul vl]", IB(make_st1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_h);
	cases.add_case(0xe4c8e861u, "st1h\t{z1.s}, p2, [x3, #-8, mul vl]", IB(make_st1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_s);
	cases.add_case(0xe4c7ebe1u, "st1h\t{z1.s}, p2, [sp, #7, mul vl]", IB(make_st1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_s);
	cases.add_case(0xe4e8e861u, "st1h\t{z1.d}, p2, [x3, #-8, mul vl]", IB(make_st1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xe4e7ebe1u, "st1h\t{z1.d}, p2, [sp, #7, mul vl]", IB(make_st1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xe4a44861u, "st1h\t{z1.h}, p2, [x3, x4, lsl #1]", IB(make_st1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xe4a44be1u, "st1h\t{z1.h}, p2, [sp, x4, lsl #1]", IB(make_st1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xe4c44861u, "st1h\t{z1.s}, p2, [x3, x4, lsl #1]", IB(make_st1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xe4c44be1u, "st1h\t{z1.s}, p2, [sp, x4, lsl #1]", IB(make_st1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xe4e44861u, "st1h\t{z1.d}, p2, [x3, x4, lsl #1]", IB(make_st1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe4e44be1u, "st1h\t{z1.d}, p2, [sp, x4, lsl #1]", IB(make_st1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe484a861u, "st1h\t{z1.d}, p2, [x3, z4.d, lsl #0]", IB(make_st1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe484abe1u, "st1h\t{z1.d}, p2, [sp, z4.d, lsl #0]", IB(make_st1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe4a4a861u, "st1h\t{z1.d}, p2, [x3, z4.d, lsl #1]", IB(make_st1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_1, aarch64::zv_d);
	cases.add_case(0xe4a4abe1u, "st1h\t{z1.d}, p2, [sp, z4.d, lsl #1]", IB(make_st1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_1, aarch64::zv_d);
	cases.add_case(0xe4e4c861u, "st1h\t{z1.s}, p2, [x3, z4.s, sxtw #1]", IB(make_st1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_1, aarch64::zv_s);
	cases.add_case(0xe4e4cbe1u, "st1h\t{z1.s}, p2, [sp, z4.s, sxtw #1]", IB(make_st1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_1, aarch64::zv_s);
	cases.add_case(0xe07f4960u, "st1h\t{za0h.h[w14, 0]}, p2, [x11]", IB(make_st1h_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::x11);
	cases.add_case(0xe0642be0u, "st1h\t{za0h.h[w13, 0]}, p2, [sp, x4, lsl #1]", IB(make_st1h_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x13, 0, aarch64::p2, aarch64::sp, aarch64::x4);
	cases.add_case(0xe06b800bu, "st1h\t{za1v.h[w12, 3]}, p0, [x0, x11, lsl #1]", IB(make_st1h_alorlprr),
	               aarch64::za, 1, aarch64::hvopt_v, aarch64::x12, 3, aarch64::p0, aarch64::x0, aarch64::x11);
	cases.add_case(0xe07f8921u, "st1h\t{za0v.h[w12, 1]}, p2, [x9]", IB(make_st1h_alorlprr), aarch64::za, 0,
	               aarch64::hvopt_v, aarch64::x12, 1, aarch64::p2, aarch64::x9, aarch64::xzr);

	return cases.validate();
}
