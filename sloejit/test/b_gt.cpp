/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("b.gt");

	cases.add_case(0x5400000cu, "b.gt\t#0", IB(make_b_gt_i), 0);
	cases.add_case(0x547fffecu, "b.gt\t#1048572", IB(make_b_gt_i), +1048572);
	cases.add_case(0x5480000cu, "b.gt\t#-1048576", IB(make_b_gt_i), -1048576);

	return cases.validate();
}
