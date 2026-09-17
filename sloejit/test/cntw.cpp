/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("cntw");

	cases.add_case(0x04a0e3e0u, "cntw\tx0", IB(make_cntw_r), aarch64::x0);
	cases.add_case(0x04a0e3feu, "cntw\tx30", IB(make_cntw_r), aarch64::x30);
	cases.add_case(0x04a0e3ffu, "cntw\txzr", IB(make_cntw_r), aarch64::xzr);

	return cases.validate();
}
