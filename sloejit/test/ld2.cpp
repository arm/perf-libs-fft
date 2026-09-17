/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld2");

	cases.add_case(0x4c408c40, "ld2\t{v0.2d, v1.2d}, [x2]", IB(make_ld2_rrr), aarch64::q0, aarch64::q1,
	               aarch64::x2, aarch64::q_type_variant::qv_2d);
	cases.add_case(0x0c4083fe, "ld2\t{v30.8b, v31.8b}, [sp]", IB(make_ld2_rrr), aarch64::q30, aarch64::q31,
	               aarch64::sp, aarch64::q_type_variant::qv_8b);

	return cases.validate();
}
