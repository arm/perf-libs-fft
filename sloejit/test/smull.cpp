/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("smull");

	cases.add_case(0x0e23c041u, "smull\tv1.8h, v2.8b, v3.8b", IB(make_smull_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h, aarch64::qv_8b);
	cases.add_case(0x0e63c041u, "smull\tv1.4s, v2.4h, v3.4h", IB(make_smull_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0ea3c041u, "smull\tv1.2d, v2.2s, v3.2s", IB(make_smull_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d, aarch64::qv_2s);

	cases.add_case(0x0f43a041u, "smull\tv1.4s, v2.4h, v3.h[0]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f53a041u, "smull\tv1.4s, v2.4h, v3.h[1]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f63a041u, "smull\tv1.4s, v2.4h, v3.h[2]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 2, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f73a041u, "smull\tv1.4s, v2.4h, v3.h[3]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f43a841u, "smull\tv1.4s, v2.4h, v3.h[4]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 4, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f53a841u, "smull\tv1.4s, v2.4h, v3.h[5]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 5, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f63a841u, "smull\tv1.4s, v2.4h, v3.h[6]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 6, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f73a841u, "smull\tv1.4s, v2.4h, v3.h[7]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f83a041u, "smull\tv1.2d, v2.2s, v3.s[0]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2d, aarch64::qv_2s);
	cases.add_case(0x0fa3a041u, "smull\tv1.2d, v2.2s, v3.s[1]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_2d, aarch64::qv_2s);
	cases.add_case(0x0f83a841u, "smull\tv1.2d, v2.2s, v3.s[2]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 2, aarch64::qv_2d, aarch64::qv_2s);
	cases.add_case(0x0fa3a841u, "smull\tv1.2d, v2.2s, v3.s[3]", IB(make_smull_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_2d, aarch64::qv_2s);

	return cases.validate();
}
