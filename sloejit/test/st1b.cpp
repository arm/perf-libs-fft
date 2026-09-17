/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("st1b");

	cases.add_case(0xe408e861u, "st1b\t{z1.b}, p2, [x3, #-8, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_b);
	cases.add_case(0xe407ebe1u, "st1b\t{z1.b}, p2, [sp, #7, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_b);
	cases.add_case(0xe428e861u, "st1b\t{z1.h}, p2, [x3, #-8, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_h);
	cases.add_case(0xe427ebe1u, "st1b\t{z1.h}, p2, [sp, #7, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_h);
	cases.add_case(0xe448e861u, "st1b\t{z1.s}, p2, [x3, #-8, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_s);
	cases.add_case(0xe447ebe1u, "st1b\t{z1.s}, p2, [sp, #7, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_s);
	cases.add_case(0xe468e861u, "st1b\t{z1.d}, p2, [x3, #-8, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xe467ebe1u, "st1b\t{z1.d}, p2, [sp, #7, mul vl]", IB(make_st1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xe4044861u, "st1b\t{z1.b}, p2, [x3, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_b);
	cases.add_case(0xe4044be1u, "st1b\t{z1.b}, p2, [sp, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_b);
	cases.add_case(0xe4244861u, "st1b\t{z1.h}, p2, [x3, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xe4244be1u, "st1b\t{z1.h}, p2, [sp, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xe4444861u, "st1b\t{z1.s}, p2, [x3, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xe4444be1u, "st1b\t{z1.s}, p2, [sp, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xe4644861u, "st1b\t{z1.d}, p2, [x3, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe4644be1u, "st1b\t{z1.d}, p2, [sp, x4]", IB(make_st1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xe404a861u, "st1b\t{z1.d}, p2, [x3, z4.d, lsl #0]", IB(make_st1b_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe404abe1u, "st1b\t{z1.d}, p2, [sp, z4.d, lsl #0]", IB(make_st1b_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe03f4960u, "st1b\t{za0h.b[w14, 0]}, p2, [x11]", IB(make_st1b_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::x11);
	cases.add_case(0xe0244be0u, "st1b\t{za0h.b[w14, 0]}, p2, [sp, x4]", IB(make_st1b_alorlprr), aarch64::za,
	               0, aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::sp, aarch64::x4);
	cases.add_case(0xe0218005u, "st1b\t{za0v.b[w12, 5]}, p0, [x0, x1]", IB(make_st1b_alorlprr), aarch64::za,
	               0, aarch64::hvopt_v, aarch64::x12, 5, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe03f8a61u, "st1b\t{za0v.b[w12, 1]}, p2, [x19]", IB(make_st1b_alorlprr), aarch64::za, 0,
	               aarch64::hvopt_v, aarch64::x12, 1, aarch64::p2, aarch64::x19, aarch64::xzr);

	return cases.validate();
}
