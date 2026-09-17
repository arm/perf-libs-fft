/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("splice");

	cases.add_case(0x05ec8120u, "splice\tz0.d, p0, z0.d, z9.d", IB(make_splice_zpz), aarch64::z0, aarch64::p0,
	               aarch64::z9, aarch64::zv_d);
	cases.add_case(0x05ac9441u, "splice\tz1.s, p5, z1.s, z2.s", IB(make_splice_zpz), aarch64::z1, aarch64::p5,
	               aarch64::z2, aarch64::zv_s);
	cases.add_case(0x056c8623u, "splice\tz3.h, p1, z3.h, z17.h", IB(make_splice_zpz), aarch64::z3,
	               aarch64::p1, aarch64::z17, aarch64::zv_h);
	cases.add_case(0x052c884bu, "splice\tz11.b, p2, z11.b, z2.b", IB(make_splice_zpz), aarch64::z11,
	               aarch64::p2, aarch64::z2, aarch64::zv_b);

	return cases.validate();
}
