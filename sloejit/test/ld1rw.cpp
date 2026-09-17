/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1rw");

	cases.add_case(0x8540c861u, "ld1rw\t{z1.s}, p2/z, [x3]", IB(make_ld1rw_zpri), aarch64::z1, aarch64::p2,
	               aarch64::x3, 0);
	cases.add_case(0x857fcbe1u, "ld1rw\t{z1.s}, p2/z, [sp, #252]", IB(make_ld1rw_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 252);

	return cases.validate();
}
