/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("smstart");

	cases.add_case(0xd503477fu, "smstart", IB(make_smstart));
	cases.add_case(0xd503437fu, "smstart\tsm", IB(make_smstart_i), aarch64::smopt_sm);

	return cases.validate();
}
