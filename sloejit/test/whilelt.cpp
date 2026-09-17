/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("whilelt");

	cases.add_case(0x25231441u, "whilelt\tp1.b, x2, x3", IB(make_whilelt_prr), aarch64::p1, aarch64::x2,
	               aarch64::x3, aarch64::zv_b);
	cases.add_case(0x253f17efu, "whilelt\tp15.b, xzr, xzr", IB(make_whilelt_prr), aarch64::p15, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_b);
	cases.add_case(0x25631441u, "whilelt\tp1.h, x2, x3", IB(make_whilelt_prr), aarch64::p1, aarch64::x2,
	               aarch64::x3, aarch64::zv_h);
	cases.add_case(0x257f17efu, "whilelt\tp15.h, xzr, xzr", IB(make_whilelt_prr), aarch64::p15, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_h);
	cases.add_case(0x25a31441u, "whilelt\tp1.s, x2, x3", IB(make_whilelt_prr), aarch64::p1, aarch64::x2,
	               aarch64::x3, aarch64::zv_s);
	cases.add_case(0x25bf17efu, "whilelt\tp15.s, xzr, xzr", IB(make_whilelt_prr), aarch64::p15, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_s);
	cases.add_case(0x25e31441u, "whilelt\tp1.d, x2, x3", IB(make_whilelt_prr), aarch64::p1, aarch64::x2,
	               aarch64::x3, aarch64::zv_d);
	cases.add_case(0x25ff17efu, "whilelt\tp15.d, xzr, xzr", IB(make_whilelt_prr), aarch64::p15, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_d);

	return cases.validate();
}
