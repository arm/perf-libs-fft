/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st3");

	cases.add_case(0x4c004864, "st3\t{v4.4s, v5.4s, v6.4s}, [x3]", IB(make_st3_rrrr), aarch64::q4,
	               aarch64::q5, aarch64::q6, aarch64::x3, aarch64::q_type_variant::qv_4s);
	cases.add_case(0x0c0047fd, "st3\t{v29.4h, v30.4h, v31.4h}, [sp]", IB(make_st3_rrrr), aarch64::q29,
	               aarch64::q30, aarch64::q31, aarch64::sp, aarch64::q_type_variant::qv_4h);

	return cases.validate();
}
