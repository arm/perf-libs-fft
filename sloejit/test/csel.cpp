/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("csel");

	cases.add_case(0x9a830041u, "csel\tx1, x2, x3, eq", IB(make_x_csel_eq_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x9a9f03ffu, "csel\txzr, xzr, xzr, eq", IB(make_x_csel_eq_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x9a831041u, "csel\tx1, x2, x3, ne", IB(make_x_csel_ne_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x9a9f13ffu, "csel\txzr, xzr, xzr, ne", IB(make_x_csel_ne_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x9a83b041u, "csel\tx1, x2, x3, lt", IB(make_x_csel_lt_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x9a9fb3ffu, "csel\txzr, xzr, xzr, lt", IB(make_x_csel_lt_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x9a83d041u, "csel\tx1, x2, x3, le", IB(make_x_csel_le_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x9a9fd3ffu, "csel\txzr, xzr, xzr, le", IB(make_x_csel_le_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x9a83c041u, "csel\tx1, x2, x3, gt", IB(make_x_csel_gt_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x9a9fc3ffu, "csel\txzr, xzr, xzr, gt", IB(make_x_csel_gt_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);
	cases.add_case(0x9a83a041u, "csel\tx1, x2, x3, ge", IB(make_x_csel_ge_rrr), aarch64::x1, aarch64::x2,
	               aarch64::x3);
	cases.add_case(0x9a9fa3ffu, "csel\txzr, xzr, xzr, ge", IB(make_x_csel_ge_rrr), aarch64::xzr, aarch64::xzr,
	               aarch64::xzr);

	return cases.validate();
}
