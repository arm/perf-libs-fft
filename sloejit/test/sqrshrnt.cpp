/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("sqrshrnt");

	cases.add_case(0x452f2c41u, "sqrshrnt\tz1.b, z2.h, #1", IB(make_sqrshrnt_zzi), aarch64::z1, aarch64::z2,
	               1u, aarch64::zv_b, aarch64::zv_h);
	cases.add_case(0x45282c83u, "sqrshrnt\tz3.b, z4.h, #8", IB(make_sqrshrnt_zzi), aarch64::z3, aarch64::z4,
	               8u, aarch64::zv_b, aarch64::zv_h);
	cases.add_case(0x453f2cc5u, "sqrshrnt\tz5.h, z6.s, #1", IB(make_sqrshrnt_zzi), aarch64::z5, aarch64::z6,
	               1u, aarch64::zv_h, aarch64::zv_s);
	cases.add_case(0x45302d07u, "sqrshrnt\tz7.h, z8.s, #16", IB(make_sqrshrnt_zzi), aarch64::z7, aarch64::z8,
	               16u, aarch64::zv_h, aarch64::zv_s);
	cases.add_case(0x457f2d49u, "sqrshrnt\tz9.s, z10.d, #1", IB(make_sqrshrnt_zzi), aarch64::z9, aarch64::z10,
	               1u, aarch64::zv_s, aarch64::zv_d);
	cases.add_case(0x45602d8bu, "sqrshrnt\tz11.s, z12.d, #32", IB(make_sqrshrnt_zzi), aarch64::z11,
	               aarch64::z12, 32u, aarch64::zv_s, aarch64::zv_d);

	return cases.validate();
}
