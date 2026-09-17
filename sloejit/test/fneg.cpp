/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fneg");

	cases.add_case(0x1ee14041u, "fneg\th1, h2", IB(make_fneg_hh), aarch64::h1, aarch64::h2);
	cases.add_case(0x1e214041u, "fneg\ts1, s2", IB(make_fneg_ss), aarch64::s1, aarch64::s2);
	cases.add_case(0x1e614041u, "fneg\td1, d2", IB(make_fneg_dd), aarch64::d1, aarch64::d2);
	cases.add_case(0x2ef8f841u, "fneg\tv1.4h, v2.4h", IB(make_fneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4h);
	cases.add_case(0x6ef8f841u, "fneg\tv1.8h, v2.8h", IB(make_fneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_8h);
	cases.add_case(0x2ea0f841u, "fneg\tv1.2s, v2.2s", IB(make_fneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2s);
	cases.add_case(0x6ea0f841u, "fneg\tv1.4s, v2.4s", IB(make_fneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4s);
	cases.add_case(0x6ee0f841u, "fneg\tv1.2d, v2.2d", IB(make_fneg_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2d);
	cases.add_case(0x045da861u, "fneg\tz1.h, p2/m, z3.h", IB(make_fneg_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_h);
	cases.add_case(0x049da861u, "fneg\tz1.s, p2/m, z3.s", IB(make_fneg_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_s);
	cases.add_case(0x04dda861u, "fneg\tz1.d, p2/m, z3.d", IB(make_fneg_zpz), aarch64::z1, aarch64::p2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
