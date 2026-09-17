/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("cnth");

	cases.add_case(0x0460e3e0u, "cnth\tx0", IB(make_cnth_r), aarch64::x0);
	cases.add_case(0x0460e3feu, "cnth\tx30", IB(make_cnth_r), aarch64::x30);
	cases.add_case(0x0460e3ffu, "cnth\txzr", IB(make_cnth_r), aarch64::xzr);

	return cases.validate();
}
