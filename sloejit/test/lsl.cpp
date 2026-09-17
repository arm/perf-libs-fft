/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("lsl");

	cases.add_case(0xd340fc22u, "lsl\tx2, x1, #0", IB(make_lsl_rri), aarch64::x2, aarch64::x1, 0);
	cases.add_case(0xd37ff822u, "lsl\tx2, x1, #1", IB(make_lsl_rri), aarch64::x2, aarch64::x1, 1);
	cases.add_case(0xd34103ffu, "lsl\txzr, xzr, #63", IB(make_lsl_rri), aarch64::xzr, aarch64::xzr, 63);

	return cases.validate();
}
