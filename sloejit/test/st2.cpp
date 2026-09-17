/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st2");

	cases.add_case(0x4c008c40, "st2\t{v0.2d, v1.2d}, [x2]", IB(make_st2_rrr), aarch64::q0, aarch64::q1,
	               aarch64::x2, aarch64::q_type_variant::qv_2d);
	cases.add_case(0x0c0083fe, "st2\t{v30.8b, v31.8b}, [sp]", IB(make_st2_rrr), aarch64::q30, aarch64::q31,
	               aarch64::sp, aarch64::q_type_variant::qv_8b);

	return cases.validate();
}
