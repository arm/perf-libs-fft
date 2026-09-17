/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ldrsb");

	cases.add_case(0x38a16843u, "ldrsb\tx3, [x2, x1]", IB(make_x_ldrsb_rrr), aarch64::x3, aarch64::x2,
	               aarch64::x1);
	cases.add_case(0x38bf6bffu, "ldrsb\txzr, [sp, xzr]", IB(make_x_ldrsb_rrr), aarch64::xzr, aarch64::sp,
	               aarch64::xzr);
	cases.add_case(0x39800041u, "ldrsb\tx1, [x2]", IB(make_x_ldrsb_rri), aarch64::x1, aarch64::x2, 0u);
	cases.add_case(0x39bffc41u, "ldrsb\tx1, [x2, #4095]", IB(make_x_ldrsb_rri), aarch64::x1, aarch64::x2,
	               4095u);
	cases.add_case(0x398003ffu, "ldrsb\txzr, [sp]", IB(make_x_ldrsb_rri), aarch64::xzr, aarch64::sp, 0u);
	cases.add_case(0x39bfffffu, "ldrsb\txzr, [sp, #4095]", IB(make_x_ldrsb_rri), aarch64::xzr, aarch64::sp,
	               4095u);

	return cases.validate();
}
