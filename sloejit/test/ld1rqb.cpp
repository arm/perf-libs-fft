/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1rqb");

	cases.add_case(0xa4082861u, "ld1rqb\t{z1.b}, p2/z, [x3, #-128]", IB(make_ld1rqb_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -128);
	cases.add_case(0xa4072be1u, "ld1rqb\t{z1.b}, p2/z, [sp, #112]", IB(make_ld1rqb_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 112);

	return cases.validate();
}
