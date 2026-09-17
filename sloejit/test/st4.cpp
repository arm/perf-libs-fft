/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st4");

	cases.add_case(0x4c0007fc, "st4\t{v28.8h, v29.8h, v30.8h, v31.8h}, [sp]", IB(make_st4_rrrrr),
	               aarch64::q28, aarch64::q29, aarch64::q30, aarch64::q31, aarch64::sp,
	               aarch64::q_type_variant::qv_8h);
	cases.add_case(0x4c000c20, "st4\t{v0.2d, v1.2d, v2.2d, v3.2d}, [x1]", IB(make_st4_rrrrr), aarch64::q0,
	               aarch64::q1, aarch64::q2, aarch64::q3, aarch64::x1, aarch64::q_type_variant::qv_2d);

	return cases.validate();
}
