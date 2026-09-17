/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1w");

	cases.add_case(0xa548a861u, "ld1w\t{z1.s}, p2/z, [x3, #-8, mul vl]", IB(make_ld1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_s);
	cases.add_case(0xa547abe1u, "ld1w\t{z1.s}, p2/z, [sp, #7, mul vl]", IB(make_ld1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_s);
	cases.add_case(0xa568a861u, "ld1w\t{z1.d}, p2/z, [x3, #-8, mul vl]", IB(make_ld1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -8, aarch64::zv_d);
	cases.add_case(0xa567abe1u, "ld1w\t{z1.d}, p2/z, [sp, #7, mul vl]", IB(make_ld1w_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 7, aarch64::zv_d);
	cases.add_case(0xa5444861u, "ld1w\t{z1.s}, p2/z, [x3, x4, lsl #2]", IB(make_ld1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xa5444be1u, "ld1w\t{z1.s}, p2/z, [sp, x4, lsl #2]", IB(make_ld1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_s);
	cases.add_case(0xa5644861u, "ld1w\t{z1.d}, p2/z, [x3, x4, lsl #2]", IB(make_ld1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xa5644be1u, "ld1w\t{z1.d}, p2/z, [sp, x4, lsl #2]", IB(make_ld1w_zprr), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::x4, aarch64::zv_d);
	cases.add_case(0xc544c861u, "ld1w\t{z1.d}, p2/z, [x3, z4.d, lsl #0]", IB(make_ld1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc544cbe1u, "ld1w\t{z1.d}, p2/z, [sp, z4.d, lsl #0]", IB(make_ld1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_0, aarch64::zv_d);
	cases.add_case(0xc564c861u, "ld1w\t{z1.d}, p2/z, [x3, z4.d, lsl #2]", IB(make_ld1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_2, aarch64::zv_d);
	cases.add_case(0xc564cbe1u, "ld1w\t{z1.d}, p2/z, [sp, z4.d, lsl #2]", IB(make_ld1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_2, aarch64::zv_d);
	cases.add_case(0x85644861u, "ld1w\t{z1.s}, p2/z, [x3, z4.s, sxtw #2]", IB(make_ld1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::x3, aarch64::z4, aarch64::lsl_2, aarch64::zv_s);
	cases.add_case(0x85644be1u, "ld1w\t{z1.s}, p2/z, [sp, z4.s, sxtw #2]", IB(make_ld1w_zprz), aarch64::z1,
	               aarch64::p2, aarch64::sp, aarch64::z4, aarch64::lsl_2, aarch64::zv_s);
	cases.add_case(0xe09f0000u, "ld1w\t{za0h.s[w12, 0]}, p0/z, [x0]", IB(make_ld1w_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0);
	cases.add_case(0xe09ff129u, "ld1w\t{za2v.s[w15, 1]}, p4/z, [x9]", IB(make_ld1w_alorlpr), aarch64::za, 2,
	               aarch64::hvopt_v, aarch64::x15, 1, aarch64::p4, aarch64::x9);
	cases.add_case(0xe0810002u, "ld1w\t{za0h.s[w12, 2]}, p0/z, [x0, x1, lsl #2]", IB(make_ld1w_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 2, aarch64::p0, aarch64::x0, aarch64::x1);
	cases.add_case(0xe09f6befu, "ld1w\t{za3h.s[w15, 3]}, p2/z, [sp]", IB(make_ld1w_alorlprr), aarch64::za, 3,
	               aarch64::hvopt_h, aarch64::x15, 3, aarch64::p2, aarch64::sp, aarch64::xzr);
	cases.add_case(0xe08901a0u, "ld1w\t{za0h.s[w12, 0]}, p0/z, [x13, x9, lsl #2]", IB(make_ld1w_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x13, aarch64::x9);

	return cases.validate();
}
