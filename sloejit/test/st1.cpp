/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("st1");

	cases.add_case(0x0d000061u, "st1\t{v1.b}[0], [x3]", IB(make_q_st1_rir), aarch64::q1, 0, aarch64::x3,
	               aarch64::qv_16b);
	cases.add_case(0x4d001fe1u, "st1\t{v1.b}[15], [sp]", IB(make_q_st1_rir), aarch64::q1, 15, aarch64::sp,
	               aarch64::qv_16b);
	cases.add_case(0x0d004061u, "st1\t{v1.h}[0], [x3]", IB(make_q_st1_rir), aarch64::q1, 0, aarch64::x3,
	               aarch64::qv_8h);
	cases.add_case(0x4d005be1u, "st1\t{v1.h}[7], [sp]", IB(make_q_st1_rir), aarch64::q1, 7, aarch64::sp,
	               aarch64::qv_8h);
	cases.add_case(0x0d008061u, "st1\t{v1.s}[0], [x3]", IB(make_q_st1_rir), aarch64::q1, 0, aarch64::x3,
	               aarch64::qv_4s);
	cases.add_case(0x4d0093e1u, "st1\t{v1.s}[3], [sp]", IB(make_q_st1_rir), aarch64::q1, 3, aarch64::sp,
	               aarch64::qv_4s);
	cases.add_case(0x0d008461u, "st1\t{v1.d}[0], [x3]", IB(make_q_st1_rir), aarch64::q1, 0, aarch64::x3,
	               aarch64::qv_2d);
	cases.add_case(0x4d0087e1u, "st1\t{v1.d}[1], [sp]", IB(make_q_st1_rir), aarch64::q1, 1, aarch64::sp,
	               aarch64::qv_2d);

	return cases.validate();
}
