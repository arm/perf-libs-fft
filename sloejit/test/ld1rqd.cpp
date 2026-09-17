/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1rqd");

	cases.add_case(0xa5882861u, "ld1rqd\t{z1.d}, p2/z, [x3, #-128]", IB(make_ld1rqd_zpri), aarch64::z1,
	               aarch64::p2, aarch64::x3, -128);
	cases.add_case(0xa5872be1u, "ld1rqd\t{z1.d}, p2/z, [sp, #112]", IB(make_ld1rqd_zpri), aarch64::z1,
	               aarch64::p2, aarch64::sp, 112);

	return cases.validate();
}
