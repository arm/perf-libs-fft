/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("rev");
	cases.add_case(0x05f83800u, "rev\tz0.d, z0.d", IB(make_rev_zz), aarch64::z0, aarch64::z0, aarch64::zv_d);
	cases.add_case(0x05b83881u, "rev\tz1.s, z4.s", IB(make_rev_zz), aarch64::z1, aarch64::z4, aarch64::zv_s);
	cases.add_case(0x05783a7fu, "rev\tz31.h, z19.h", IB(make_rev_zz), aarch64::z31, aarch64::z19,
	               aarch64::zv_h);
	cases.add_case(0x05383a4bu, "rev\tz11.b, z18.b", IB(make_rev_zz), aarch64::z11, aarch64::z18,
	               aarch64::zv_b);

	cases.add_case(0x05f441e0u, "rev\tp0.d, p15.d", IB(make_rev_pp), aarch64::p0, aarch64::p15,
	               aarch64::zv_d);
	cases.add_case(0x05b44107u, "rev\tp7.s, p8.s", IB(make_rev_pp), aarch64::p7, aarch64::p8, aarch64::zv_s);
	cases.add_case(0x05744021u, "rev\tp1.h, p1.h", IB(make_rev_pp), aarch64::p1, aarch64::p1, aarch64::zv_h);
	cases.add_case(0x05344049u, "rev\tp9.b, p2.b", IB(make_rev_pp), aarch64::p9, aarch64::p2, aarch64::zv_b);

	return cases.validate();
}
