/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1rqw");

	cases.add_case(0xa5082861u, "ld1rqw\t{z1.s}, p2/z, [x3, #-128]", IB(make_ld1rqw_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -128);
	cases.add_case(0xa5072be1u, "ld1rqw\t{z1.s}, p2/z, [sp, #112]", IB(make_ld1rqw_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 112);

	return cases.validate();
}
