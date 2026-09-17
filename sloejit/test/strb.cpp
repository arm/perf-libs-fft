/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("strb");

	cases.add_case(0x38216843u, "strb\tw3, [x2, x1]", IB(make_x_strb_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0x383f6bffu, "strb\twzr, [sp, xzr]", IB(make_x_strb_rrr), aarch64::xzr, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x39000043u, "strb\tw3, [x2]", IB(make_x_strb_rri), aarch64::x3, aarch64::x2, 0u);
	cases.add_case(0x393ffc43u, "strb\tw3, [x2, #4095]", IB(make_x_strb_rri), aarch64::x3, aarch64::x2,
	               4095u);
	cases.add_case(0x390003ffu, "strb\twzr, [sp]", IB(make_x_strb_rri), aarch64::xzr, aarch64::sp, 0u);
	cases.add_case(0x393fffffu, "strb\twzr, [sp, #4095]", IB(make_x_strb_rri), aarch64::xzr, aarch64::sp,
	               4095u);

	return cases.validate();
}
