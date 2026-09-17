/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("adr");

	cases.add_case(0x707fffe1u, "adr\tx1, #1048575", IB(make_adr_ri), aarch64::x1, 1048575);
	cases.add_case(0x1080001fu, "adr\txzr, #-1048576", IB(make_adr_ri), aarch64::xzr, -1048576);

	return cases.validate();
}
