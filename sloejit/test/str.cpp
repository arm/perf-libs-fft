/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("str");

	cases.add_case(0xf8216843u, "str\tx3, [x2, x1]", IB(make_x_str_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0xf83f6bffu, "str\txzr, [sp, xzr]", IB(make_x_str_rrr), aarch64::xzr, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0xf9000043u, "str\tx3, [x2]", IB(make_x_str_rri), aarch64::x3, aarch64::x2, 0u);
	cases.add_case(0xf93ffc43u, "str\tx3, [x2, #32760]", IB(make_x_str_rri), aarch64::x3, aarch64::x2,
	               32760u);
	cases.add_case(0xf90003ffu, "str\txzr, [sp]", IB(make_x_str_rri), aarch64::xzr, aarch64::sp, 0u);
	cases.add_case(0xf93fffffu, "str\txzr, [sp, #32760]", IB(make_x_str_rri), aarch64::xzr, aarch64::sp,
	               32760u);
	cases.add_case(0x3d000041u, "str\tb1, [x2]", IB(make_b_str_rri), aarch64::b1, aarch64::x2, 0);
	cases.add_case(0x3d3fffe1u, "str\tb1, [sp, #4095]", IB(make_b_str_rri), aarch64::b1, aarch64::sp, 4095);
	cases.add_case(0x7d000041u, "str\th1, [x2]", IB(make_h_str_rri), aarch64::h1, aarch64::x2, 0);
	cases.add_case(0x7d3fffe1u, "str\th1, [sp, #8190]", IB(make_h_str_rri), aarch64::h1, aarch64::sp, 8190);
	cases.add_case(0xbd000041u, "str\ts1, [x2]", IB(make_s_str_rri), aarch64::s1, aarch64::x2, 0);
	cases.add_case(0xbd3fffe1u, "str\ts1, [sp, #16380]", IB(make_s_str_rri), aarch64::s1, aarch64::sp, 16380);
	cases.add_case(0xfd000041u, "str\td1, [x2]", IB(make_d_str_rri), aarch64::d1, aarch64::x2, 0);
	cases.add_case(0xfd3fffe1u, "str\td1, [sp, #32760]", IB(make_d_str_rri), aarch64::d1, aarch64::sp, 32760);
	cases.add_case(0x3d800041u, "str\tq1, [x2]", IB(make_q_str_rri), aarch64::q1, aarch64::x2, 0);
	cases.add_case(0x3dbfffe1u, "str\tq1, [sp, #65520]", IB(make_q_str_rri), aarch64::q1, aarch64::sp, 65520);
	cases.add_case(0x3c236841u, "str\tb1, [x2, x3]", IB(make_b_str_rrr), aarch64::b1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x3c3f6be1u, "str\tb1, [sp, xzr]", IB(make_b_str_rrr), aarch64::b1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x7c237841u, "str\th1, [x2, x3, lsl #1]", IB(make_h_str_rrr), aarch64::h1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x7c3f7be1u, "str\th1, [sp, xzr, lsl #1]", IB(make_h_str_rrr), aarch64::h1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0xbc237841u, "str\ts1, [x2, x3, lsl #2]", IB(make_s_str_rrr), aarch64::s1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0xbc3f7be1u, "str\ts1, [sp, xzr, lsl #2]", IB(make_s_str_rrr), aarch64::s1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0xfc237841u, "str\td1, [x2, x3, lsl #3]", IB(make_d_str_rrr), aarch64::d1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0xfc3f7be1u, "str\td1, [sp, xzr, lsl #3]", IB(make_d_str_rrr), aarch64::d1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x3ca37841u, "str\tq1, [x2, x3, lsl #4]", IB(make_q_str_rrr), aarch64::q1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x3cbf7be1u, "str\tq1, [sp, xzr, lsl #4]", IB(make_q_str_rrr), aarch64::q1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0xe59f5fe1u, "str\tz1, [sp, #255, mul vl]", IB(make_str_zri), aarch64::z1, aarch64::sp,
	               255);

	return cases.validate();
}
