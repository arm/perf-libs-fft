/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("incw");

	cases.add_case(0x04b0e3e0u, "incw\tx0", IB(make_incw_r), aarch64::x0);
	cases.add_case(0x04b0e3feu, "incw\tx30", IB(make_incw_r), aarch64::x30);
	cases.add_case(0x04b0e3ffu, "incw\txzr", IB(make_incw_r), aarch64::xzr);

	return cases.validate();
}
