/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ldr");

	cases.add_case(0xf8616843u, "ldr\tx3, [x2, x1]", IB(make_x_ldr_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0xf8616bffu, "ldr\txzr, [sp, x1]", IB(make_x_ldr_rrr), aarch64::xzr, aarch64::sp,
	               aarch64::x1);
	cases.add_case(0xf9400041u, "ldr\tx1, [x2]", IB(make_x_ldr_rri), aarch64::x1, aarch64::x2, 0u);
	cases.add_case(0xf97ffc41u, "ldr\tx1, [x2, #32760]", IB(make_x_ldr_rri), aarch64::x1, aarch64::x2,
	               32760u);
	cases.add_case(0xf94003ffu, "ldr\txzr, [sp]", IB(make_x_ldr_rri), aarch64::xzr, aarch64::sp, 0u);
	cases.add_case(0xf97fffffu, "ldr\txzr, [sp, #32760]", IB(make_x_ldr_rri), aarch64::xzr, aarch64::sp,
	               32760u);
	cases.add_case(0x3d400041u, "ldr\tb1, [x2]", IB(make_b_ldr_rri), aarch64::b1, aarch64::x2, 0);
	cases.add_case(0x3d7fffe1u, "ldr\tb1, [sp, #4095]", IB(make_b_ldr_rri), aarch64::b1, aarch64::sp, 4095);
	cases.add_case(0x7d400041u, "ldr\th1, [x2]", IB(make_h_ldr_rri), aarch64::h1, aarch64::x2, 0);
	cases.add_case(0x7d7fffe1u, "ldr\th1, [sp, #8190]", IB(make_h_ldr_rri), aarch64::h1, aarch64::sp, 8190);
	cases.add_case(0xbd400041u, "ldr\ts1, [x2]", IB(make_s_ldr_rri), aarch64::s1, aarch64::x2, 0);
	cases.add_case(0xbd7fffe1u, "ldr\ts1, [sp, #16380]", IB(make_s_ldr_rri), aarch64::s1, aarch64::sp, 16380);
	cases.add_case(0xfd400041u, "ldr\td1, [x2]", IB(make_d_ldr_rri), aarch64::d1, aarch64::x2, 0);
	cases.add_case(0xfd7fffe1u, "ldr\td1, [sp, #32760]", IB(make_d_ldr_rri), aarch64::d1, aarch64::sp, 32760);
	cases.add_case(0x3dc00041u, "ldr\tq1, [x2]", IB(make_q_ldr_rri), aarch64::q1, aarch64::x2, 0);
	cases.add_case(0x3dffffe1u, "ldr\tq1, [sp, #65520]", IB(make_q_ldr_rri), aarch64::q1, aarch64::sp, 65520);
	cases.add_case(0x3c636841u, "ldr\tb1, [x2, x3]", IB(make_b_ldr_rrr), aarch64::b1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x3c7f6be1u, "ldr\tb1, [sp, xzr]", IB(make_b_ldr_rrr), aarch64::b1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x7c637841u, "ldr\th1, [x2, x3, lsl #1]", IB(make_h_ldr_rrr), aarch64::h1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x7c7f7be1u, "ldr\th1, [sp, xzr, lsl #1]", IB(make_h_ldr_rrr), aarch64::h1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0xbc637841u, "ldr\ts1, [x2, x3, lsl #2]", IB(make_s_ldr_rrr), aarch64::s1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0xbc7f7be1u, "ldr\ts1, [sp, xzr, lsl #2]", IB(make_s_ldr_rrr), aarch64::s1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0xfc637841u, "ldr\td1, [x2, x3, lsl #3]", IB(make_d_ldr_rrr), aarch64::d1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0xfc7f7be1u, "ldr\td1, [sp, xzr, lsl #3]", IB(make_d_ldr_rrr), aarch64::d1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x3ce37841u, "ldr\tq1, [x2, x3, lsl #4]", IB(make_q_ldr_rrr), aarch64::q1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x3cff7be1u, "ldr\tq1, [sp, xzr, lsl #4]", IB(make_q_ldr_rrr), aarch64::q1, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x85a04041u, "ldr\tz1, [x2, #-256, mul vl]", IBO(make_ldr_zri, reg, reg, int32_t),
	               aarch64::z1, aarch64::x2, -256);
	cases.add_case(0x859f5fe1u, "ldr\tz1, [sp, #255, mul vl]", IBO(make_ldr_zri, reg, reg, int32_t),
	               aarch64::z1, aarch64::sp, 255);
	cases.add_case(0x85a04041u, "ldr\tz1, [x2, #-256, mul vl]", IBO(make_ldr_zri, reg, reg, int32_t),
	               aarch64::z1, aarch64::x2, -256);

	return cases.validate();
}
