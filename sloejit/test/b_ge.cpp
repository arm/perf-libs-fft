/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("b.ge");

	cases.add_case(0x5400000au, "b.ge\t#0", IB(make_b_ge_i), 0);
	cases.add_case(0x547fffeau, "b.ge\t#1048572", IB(make_b_ge_i), +1048572);
	cases.add_case(0x5480000au, "b.ge\t#-1048576", IB(make_b_ge_i), -1048576);

	return cases.validate();
}
