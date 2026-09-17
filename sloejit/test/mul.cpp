/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("mul");

	cases.add_case(0x04100861u, "mul\tz1.b, p2/m, z1.b, z3.b", IB(make_mul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04500861u, "mul\tz1.h, p2/m, z1.h, z3.h", IB(make_mul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x04900861u, "mul\tz1.s, p2/m, z1.s, z3.s", IB(make_mul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x04d00861u, "mul\tz1.d, p2/m, z1.d, z3.d", IB(make_mul_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
