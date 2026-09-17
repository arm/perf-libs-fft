/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ldp");

	cases.add_case(0xa9400861u, "ldp\tx1, x2, [x3]", IB(make_x_ldp_rrri), aarch64::x1, aarch64::x2,
	               aarch64::x3, 0);
	cases.add_case(0xa9600861u, "ldp\tx1, x2, [x3, #-512]", IB(make_x_ldp_rrri), aarch64::x1, aarch64::x2,
	               aarch64::x3, -512);
	cases.add_case(0xa95f8861u, "ldp\tx1, x2, [x3, #504]", IB(make_x_ldp_rrri), aarch64::x1, aarch64::x2,
	               aarch64::x3, 504);
	cases.add_case(0xa9407fe0u, "ldp\tx0, xzr, [sp]", IB(make_x_ldp_rrri), aarch64::x0, aarch64::xzr,
	               aarch64::sp, 0);
	cases.add_case(0xa9c00861u, "ldp\tx1, x2, [x3, #0]!", IB(make_x_ldp_preindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 0);
	cases.add_case(0xa9e00861u, "ldp\tx1, x2, [x3, #-512]!", IB(make_x_ldp_preindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, -512);
	cases.add_case(0xa9df8861u, "ldp\tx1, x2, [x3, #504]!", IB(make_x_ldp_preindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 504);
	cases.add_case(0xa9c07fe0u, "ldp\tx0, xzr, [sp, #0]!", IB(make_x_ldp_preindex_rrri), aarch64::x0,
	               aarch64::xzr, aarch64::sp, 0);
	cases.add_case(0xa8c00861u, "ldp\tx1, x2, [x3], #0", IB(make_x_ldp_postindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 0);
	cases.add_case(0xa8e00861u, "ldp\tx1, x2, [x3], #-512", IB(make_x_ldp_postindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, -512);
	cases.add_case(0xa8df8861u, "ldp\tx1, x2, [x3], #504", IB(make_x_ldp_postindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 504);
	cases.add_case(0xa8c07fe0u, "ldp\tx0, xzr, [sp], #0", IB(make_x_ldp_postindex_rrri), aarch64::x0,
	               aarch64::xzr, aarch64::sp, 0);
	cases.add_case(0x2d400861u, "ldp\ts1, s2, [x3]", IB(make_s_ldp_rrri), aarch64::s1, aarch64::s2,
	               aarch64::x3, 0);
	cases.add_case(0x2d600861u, "ldp\ts1, s2, [x3, #-256]", IB(make_s_ldp_rrri), aarch64::s1, aarch64::s2,
	               aarch64::x3, -256);
	cases.add_case(0x2d5f8be1u, "ldp\ts1, s2, [sp, #252]", IB(make_s_ldp_rrri), aarch64::s1, aarch64::s2,
	               aarch64::sp, 252);
	cases.add_case(0x6d400861u, "ldp\td1, d2, [x3]", IB(make_d_ldp_rrri), aarch64::d1, aarch64::d2,
	               aarch64::x3, 0);
	cases.add_case(0x6d600861u, "ldp\td1, d2, [x3, #-512]", IB(make_d_ldp_rrri), aarch64::d1, aarch64::d2,
	               aarch64::x3, -512);
	cases.add_case(0x6d5f8be1u, "ldp\td1, d2, [sp, #504]", IB(make_d_ldp_rrri), aarch64::d1, aarch64::d2,
	               aarch64::sp, 504);
	cases.add_case(0xad400861u, "ldp\tq1, q2, [x3]", IB(make_q_ldp_rrri), aarch64::q1, aarch64::q2,
	               aarch64::x3, 0);
	cases.add_case(0xad600861u, "ldp\tq1, q2, [x3, #-1024]", IB(make_q_ldp_rrri), aarch64::q1, aarch64::q2,
	               aarch64::x3, -1024);
	cases.add_case(0xad5f8be1u, "ldp\tq1, q2, [sp, #1008]", IB(make_q_ldp_rrri), aarch64::q1, aarch64::q2,
	               aarch64::sp, 1008);

	return cases.validate();
}
