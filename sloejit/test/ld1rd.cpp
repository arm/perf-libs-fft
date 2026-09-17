/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1rd");

	cases.add_case(0x85c0e861u, "ld1rd\t{z1.d}, p2/z, [x3]", IB(make_ld1rd_zpri), aarch64::z1, aarch64::p2,
	               aarch64::x3, 0);
	cases.add_case(0x85ffebe1u, "ld1rd\t{z1.d}, p2/z, [sp, #504]", IB(make_ld1rd_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 504);

	return cases.validate();
}
