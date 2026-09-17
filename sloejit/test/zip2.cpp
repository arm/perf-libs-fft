/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("zip2");

	cases.add_case(0x0e037841u, "zip2\tv1.8b, v2.8b, v3.8b", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8b);
	cases.add_case(0x4e037841u, "zip2\tv1.16b, v2.16b, v3.16b", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_16b);
	cases.add_case(0x0e437841u, "zip2\tv1.4h, v2.4h, v3.4h", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x4e437841u, "zip2\tv1.8h, v2.8h, v3.8h", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x0e837841u, "zip2\tv1.2s, v2.2s, v3.2s", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4e837841u, "zip2\tv1.4s, v2.4s, v3.4s", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4ec37841u, "zip2\tv1.2d, v2.2d, v3.2d", IB(make_zip2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x05234441u, "zip2\tp1.b, p2.b, p3.b", IB(make_zip2_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_b);
	cases.add_case(0x05634441u, "zip2\tp1.h, p2.h, p3.h", IB(make_zip2_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_h);
	cases.add_case(0x05a34441u, "zip2\tp1.s, p2.s, p3.s", IB(make_zip2_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_s);
	cases.add_case(0x05e34441u, "zip2\tp1.d, p2.d, p3.d", IB(make_zip2_ppp), aarch64::p1, aarch64::p2,
	               aarch64::p3, aarch64::zv_d);
	cases.add_case(0x05236441u, "zip2\tz1.b, z2.b, z3.b", IB(make_zip2_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x05636441u, "zip2\tz1.h, z2.h, z3.h", IB(make_zip2_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x05a36441u, "zip2\tz1.s, z2.s, z3.s", IB(make_zip2_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x05e36441u, "zip2\tz1.d, z2.d, z3.d", IB(make_zip2_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
