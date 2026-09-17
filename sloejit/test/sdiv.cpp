/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sdiv");

	cases.add_case(0x9ac10c43u, "sdiv\tx3, x2, x1", IB(make_x_sdiv_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0x9adf0fffu, "sdiv\txzr, xzr, xzr", IB(make_x_sdiv_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);

	return cases.validate();
}
