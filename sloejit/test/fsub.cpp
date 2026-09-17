/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fsub");

	cases.add_case(0x1ee33841u, "fsub\th1, h2, h3", IB(make_fsub_hhh), aarch64::h1, aarch64::h2, aarch64::h3);
	cases.add_case(0x1e233841u, "fsub\ts1, s2, s3", IB(make_fsub_sss), aarch64::s1, aarch64::s2, aarch64::s3);
	cases.add_case(0x1e633841u, "fsub\td1, d2, d3", IB(make_fsub_ddd), aarch64::d1, aarch64::d2, aarch64::d3);
	cases.add_case(0x0ec31441u, "fsub\tv1.4h, v2.4h, v3.4h", IB(make_fsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x4ec31441u, "fsub\tv1.8h, v2.8h, v3.8h", IB(make_fsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x0ea3d441u, "fsub\tv1.2s, v2.2s, v3.2s", IB(make_fsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4ea3d441u, "fsub\tv1.4s, v2.4s, v3.4s", IB(make_fsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4ee3d441u, "fsub\tv1.2d, v2.2d, v3.2d", IB(make_fsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x65430441u, "fsub\tz1.h, z2.h, z3.h", IB(make_fsub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x65830441u, "fsub\tz1.s, z2.s, z3.s", IB(make_fsub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x65c30441u, "fsub\tz1.d, z2.d, z3.d", IB(make_fsub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);
	cases.add_case(0x65418861u, "fsub\tz1.h, p2/m, z1.h, z3.h", IB(make_fsub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x65818861u, "fsub\tz1.s, p2/m, z1.s, z3.s", IB(make_fsub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x65c18861u, "fsub\tz1.d, p2/m, z1.d, z3.d", IB(make_fsub_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
