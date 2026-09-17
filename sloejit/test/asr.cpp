/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("asr");

	cases.add_case(0x9340fc22u, "asr\tx2, x1, #0", IB(make_asr_rri), aarch64::x2, aarch64::x1, 0);
	cases.add_case(0x9341fc22u, "asr\tx2, x1, #1", IB(make_asr_rri), aarch64::x2, aarch64::x1, 1);
	cases.add_case(0x937fffffu, "asr\txzr, xzr, #63", IB(make_asr_rri), aarch64::xzr, aarch64::xzr, 63);

	return cases.validate();
}
