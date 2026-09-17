/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1h");

	cases.add_case(0xa4a8a861u, "ld1h\t{z1.h}, p2/z, [x3, #-8, mul vl]", IB(make_ld1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_h);
	cases.add_case(0xa4a7abe1u, "ld1h\t{z1.h}, p2/z, [sp, #7, mul vl]", IB(make_ld1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_h);
	cases.add_case(0xa4c8a861u, "ld1h\t{z1.s}, p2/z, [x3, #-8, mul vl]", IB(make_ld1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_s);
	cases.add_case(0xa4c7abe1u, "ld1h\t{z1.s}, p2/z, [sp, #7, mul vl]", IB(make_ld1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_s);
	cases.add_case(0xa4e8a861u, "ld1h\t{z1.d}, p2/z, [x3, #-8, mul vl]", IB(make_ld1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xa4e7abe1u, "ld1h\t{z1.d}, p2/z, [sp, #7, mul vl]", IB(make_ld1h_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xa4a44861u, "ld1h\t{z1.h}, p2/z, [x3, x4, lsl #1]", IB(make_ld1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xa4a44be1u, "ld1h\t{z1.h}, p2/z, [sp, x4, lsl #1]", IB(make_ld1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_h);
	cases.add_case(0xa4c44861u, "ld1h\t{z1.s}, p2/z, [x3, x4, lsl #1]", IB(make_ld1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xa4c44be1u, "ld1h\t{z1.s}, p2/z, [sp, x4, lsl #1]", IB(make_ld1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xa4e44861u, "ld1h\t{z1.d}, p2/z, [x3, x4, lsl #1]", IB(make_ld1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xa4e44be1u, "ld1h\t{z1.d}, p2/z, [sp, x4, lsl #1]", IB(make_ld1h_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xc4c4c861u, "ld1h\t{z1.d}, p2/z, [x3, z4.d, lsl #0]", IB(make_ld1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc4c4cbe1u, "ld1h\t{z1.d}, p2/z, [sp, z4.d, lsl #0]", IB(make_ld1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc4e4c861u, "ld1h\t{z1.d}, p2/z, [x3, z4.d, lsl #1]", IB(make_ld1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_1, aarch64::zv_d);
	cases.add_case(0xc4e4cbe1u, "ld1h\t{z1.d}, p2/z, [sp, z4.d, lsl #1]", IB(make_ld1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_1, aarch64::zv_d);
	cases.add_case(0x84e44861u, "ld1h\t{z1.s}, p2/z, [x3, z4.s, sxtw #1]", IB(make_ld1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_1, aarch64::zv_s);
	cases.add_case(0x84e44be1u, "ld1h\t{z1.s}, p2/z, [sp, z4.s, sxtw #1]", IB(make_ld1h_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_1, aarch64::zv_s);
	cases.add_case(0xe05f0000u, "ld1h\t{za0h.h[w12, 0]}, p0/z, [x0]", IB(make_ld1h_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0);
	cases.add_case(0xe05ff129u, "ld1h\t{za1v.h[w15, 1]}, p4/z, [x9]", IB(make_ld1h_alorlpr), aarch64::za, 1,
	               aarch64::hvopt_v, aarch64::x15, 1, aarch64::p4, aarch64::x9);
	cases.add_case(0xe05f6be2u, "ld1h\t{za0h.h[w15, 2]}, p2/z, [sp]", IB(make_ld1h_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x15, 2, aarch64::p2, aarch64::sp);
	cases.add_case(0xe041000fu, "ld1h\t{za1h.h[w12, 7]}, p0/z, [x0, x1, lsl #1]", IB(make_ld1h_alorlprr),
	               aarch64::za, 1, aarch64::hvopt_h, aarch64::x12, 7, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe05f77e3u, "ld1h\t{za0h.h[w15, 3]}, p5/z, [sp]", IB(make_ld1h_alorlprr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x15, 3, aarch64::p5, aarch64::sp, aarch64::xzr);
	cases.add_case(0xe04901a8u, "ld1h\t{za1h.h[w12, 0]}, p0/z, [x13, x9, lsl #1]", IB(make_ld1h_alorlprr),
	               aarch64::za, 1, aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x13, aarch64::x9);

	return cases.validate();
}
