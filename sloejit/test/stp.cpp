/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("stp");

	cases.add_case(0xa9000861u, "stp\tx1, x2, [x3]", IB(make_x_stp_rrri), aarch64::x1, aarch64::x2,
	               aarch64::x3, 0);
	cases.add_case(0xa9200861u, "stp\tx1, x2, [x3, #-512]", IB(make_x_stp_rrri), aarch64::x1, aarch64::x2,
	               aarch64::x3, -512);
	cases.add_case(0xa91f8861u, "stp\tx1, x2, [x3, #504]", IB(make_x_stp_rrri), aarch64::x1, aarch64::x2,
	               aarch64::x3, 504);
	cases.add_case(0xa9007fffu, "stp\txzr, xzr, [sp]", IB(make_x_stp_rrri), aarch64::xzr, aarch64::xzr,
	               aarch64::sp, 0);
	cases.add_case(0xa9800861u, "stp\tx1, x2, [x3, #0]!", IB(make_x_stp_preindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 0);
	cases.add_case(0xa9a00861u, "stp\tx1, x2, [x3, #-512]!", IB(make_x_stp_preindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, -512);
	cases.add_case(0xa99f8861u, "stp\tx1, x2, [x3, #504]!", IB(make_x_stp_preindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 504);
	cases.add_case(0xa8807fffu, "stp\txzr, xzr, [sp], #0", IB(make_x_stp_postindex_rrri), aarch64::xzr,
	               aarch64::xzr, aarch64::sp, 0);
	cases.add_case(0xa8800861u, "stp\tx1, x2, [x3], #0", IB(make_x_stp_postindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 0);
	cases.add_case(0xa8a00861u, "stp\tx1, x2, [x3], #-512", IB(make_x_stp_postindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, -512);
	cases.add_case(0xa89f8861u, "stp\tx1, x2, [x3], #504", IB(make_x_stp_postindex_rrri), aarch64::x1,
	               aarch64::x2, aarch64::x3, 504);
	cases.add_case(0xa8807fffu, "stp\txzr, xzr, [sp], #0", IB(make_x_stp_postindex_rrri), aarch64::xzr,
	               aarch64::xzr, aarch64::sp, 0);

	return cases.validate();
}
