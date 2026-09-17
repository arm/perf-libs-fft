/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1rb");

	cases.add_case(0x84408861u, "ld1rb\t{z1.b}, p2/z, [x3]", IB(make_ld1rb_zpri), aarch64::z1, aarch64::p2,
	               aarch64::x3, 0);
	cases.add_case(0x847f8be1u, "ld1rb\t{z1.b}, p2/z, [sp, #63]", IB(make_ld1rb_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 63);

	return cases.validate();
}
