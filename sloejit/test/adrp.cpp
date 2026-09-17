/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("adrp");

	cases.add_case(0xf07fffe1u, "adrp\tx1, #4294963200", IB(make_adrp_ri), aarch64::x1, 1048575 * 4096ll);
	cases.add_case(0x9080001fu, "adrp\txzr, #-4294967296", IB(make_adrp_ri), aarch64::xzr, -1048576 * 4096ll);

	return cases.validate();
}
