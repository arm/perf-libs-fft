/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1b");

	cases.add_case(0xa408a861u, "ld1b\t{z1.b}, p2/z, [x3, #-8, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_b);
	cases.add_case(0xa407abe1u, "ld1b\t{z1.b}, p2/z, [sp, #7, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_b);
	cases.add_case(0xa428a861u, "ld1b\t{z1.h}, p2/z, [x3, #-8, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_h);
	cases.add_case(0xa427abe1u, "ld1b\t{z1.h}, p2/z, [sp, #7, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_h);
	cases.add_case(0xa448a861u, "ld1b\t{z1.s}, p2/z, [x3, #-8, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_s);
	cases.add_case(0xa447abe1u, "ld1b\t{z1.s}, p2/z, [sp, #7, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_s);
	cases.add_case(0xa468a861u, "ld1b\t{z1.d}, p2/z, [x3, #-8, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xa467abe1u, "ld1b\t{z1.d}, p2/z, [sp, #7, mul vl]", IB(make_ld1b_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xa4044861u, "ld1b\t{z1.b}, p2/z, [x3, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_b);
	cases.add_case(0xa4044be1u, "ld1b\t{z1.b}, p2/z, [sp, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_b);
	cases.add_case(0xa4244861u, "ld1b\t{z1.h}, p2/z, [x3, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xa4244be1u, "ld1b\t{z1.h}, p2/z, [sp, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xa4444861u, "ld1b\t{z1.s}, p2/z, [x3, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xa4444be1u, "ld1b\t{z1.s}, p2/z, [sp, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xa4644861u, "ld1b\t{z1.d}, p2/z, [x3, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xa4644be1u, "ld1b\t{z1.d}, p2/z, [sp, x4]", IB(make_ld1b_zprr), aarch64::z1, aarch64::p2,
	               aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xc444c861u, "ld1b\t{z1.d}, p2/z, [x3, z4.d, lsl #0]", IB(make_ld1b_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc444cbe1u, "ld1b\t{z1.d}, p2/z, [sp, z4.d, lsl #0]", IB(make_ld1b_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xe01f0000u, "ld1b\t{za0h.b[w12, 0]}, p0/z, [x0]", IB(make_ld1b_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0);
	cases.add_case(0xe01ff121u, "ld1b\t{za0v.b[w15, 1]}, p4/z, [x9]", IB(make_ld1b_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_v, aarch64::x15, 1, aarch64::p4, aarch64::x9);
	cases.add_case(0xe01f6be2u, "ld1b\t{za0h.b[w15, 2]}, p2/z, [sp]", IB(make_ld1b_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x15, 2, aarch64::p2, aarch64::sp);
	cases.add_case(0xe001000du, "ld1b\t{za0h.b[w12, 13]}, p0/z, [x0, x1]", IB(make_ld1b_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 13, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe01f6befu, "ld1b\t{za0h.b[w15, 15]}, p2/z, [sp]", IB(make_ld1b_alorlprr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x15, 15, aarch64::p2, aarch64::sp, aarch64::xzr);
	cases.add_case(0xe00901a0u, "ld1b\t{za0h.b[w12, 0]}, p0/z, [x13, x9]", IB(make_ld1b_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x13, aarch64::x9);

	return cases.validate();
}
