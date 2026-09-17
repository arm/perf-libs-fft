/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("trn1");

	cases.add_case(0x0e032841u, "trn1\tv1.8b, v2.8b, v3.8b", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8b);
	cases.add_case(0x4e032841u, "trn1\tv1.16b, v2.16b, v3.16b", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_16b);
	cases.add_case(0x0e432841u, "trn1\tv1.4h, v2.4h, v3.4h", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x4e432841u, "trn1\tv1.8h, v2.8h, v3.8h", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x0e832841u, "trn1\tv1.2s, v2.2s, v3.2s", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4e832841u, "trn1\tv1.4s, v2.4s, v3.4s", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4ec32841u, "trn1\tv1.2d, v2.2d, v3.2d", IB(make_trn1_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x05235041u, "trn1\tp1.b, p2.b, p3.b", IB(make_trn1_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_b);
	cases.add_case(0x05635041u, "trn1\tp1.h, p2.h, p3.h", IB(make_trn1_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_h);
	cases.add_case(0x05a35041u, "trn1\tp1.s, p2.s, p3.s", IB(make_trn1_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_s);
	cases.add_case(0x05e35041u, "trn1\tp1.d, p2.d, p3.d", IB(make_trn1_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_d);
	cases.add_case(0x05237041u, "trn1\tz1.b, z2.b, z3.b", IB(make_trn1_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x05637041u, "trn1\tz1.h, z2.h, z3.h", IB(make_trn1_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x05a37041u, "trn1\tz1.s, z2.s, z3.s", IB(make_trn1_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x05e37041u, "trn1\tz1.d, z2.d, z3.d", IB(make_trn1_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
