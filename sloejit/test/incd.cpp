/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("incd");

	cases.add_case(0x04f0e3e0u, "incd\tx0", IB(make_incd_r), aarch64::x0);
	cases.add_case(0x04f0e3feu, "incd\tx30", IB(make_incd_r), aarch64::x30);
	cases.add_case(0x04f0e3ffu, "incd\txzr", IB(make_incd_r), aarch64::xzr);

	return cases.validate();
}
