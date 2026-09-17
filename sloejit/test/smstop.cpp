/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("smstop");

	cases.add_case(0xd503467fu, "smstop", IB(make_smstop));
	cases.add_case(0xd503447fu, "smstop\tza", IB(make_smstop_i), aarch64::smopt_za);

	return cases.validate();
}
