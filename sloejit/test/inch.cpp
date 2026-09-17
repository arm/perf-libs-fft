/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("inch");

	cases.add_case(0x0470e3e0u, "inch\tx0", IB(make_inch_r), aarch64::x0);
	cases.add_case(0x0470e3feu, "inch\tx30", IB(make_inch_r), aarch64::x30);
	cases.add_case(0x0470e3ffu, "inch\txzr", IB(make_inch_r), aarch64::xzr);

	return cases.validate();
}
