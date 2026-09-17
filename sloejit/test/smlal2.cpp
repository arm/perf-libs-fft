/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("smlal2");

	cases.add_case(0x4e238041u, "smlal2\tv1.8h, v2.16b, v3.16b", IB(make_smlal2_qqq), aarch64::q1,
	               aarch64::q2, aarch64::q3, aarch64::qv_8h, aarch64::qv_16b);
	cases.add_case(0x4e638041u, "smlal2\tv1.4s, v2.8h, v3.8h", IB(make_smlal2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4ea38041u, "smlal2\tv1.2d, v2.4s, v3.4s", IB(make_smlal2_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d, aarch64::qv_4s);

	cases.add_case(0x4f432041u, "smlal2\tv1.4s, v2.8h, v3.h[0]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f532041u, "smlal2\tv1.4s, v2.8h, v3.h[1]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f632041u, "smlal2\tv1.4s, v2.8h, v3.h[2]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 2, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f732041u, "smlal2\tv1.4s, v2.8h, v3.h[3]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 3, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f432841u, "smlal2\tv1.4s, v2.8h, v3.h[4]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 4, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f532841u, "smlal2\tv1.4s, v2.8h, v3.h[5]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 5, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f632841u, "smlal2\tv1.4s, v2.8h, v3.h[6]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 6, aarch64::qv_4s, aarch64::qv_8h);
	cases.add_case(0x4f732841u, "smlal2\tv1.4s, v2.8h, v3.h[7]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 7, aarch64::qv_4s, aarch64::qv_8h);

	cases.add_case(0x4f832041u, "smlal2\tv1.2d, v2.4s, v3.s[0]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 0, aarch64::qv_2d, aarch64::qv_4s);
	cases.add_case(0x4fa32041u, "smlal2\tv1.2d, v2.4s, v3.s[1]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 1, aarch64::qv_2d, aarch64::qv_4s);
	cases.add_case(0x4f832841u, "smlal2\tv1.2d, v2.4s, v3.s[2]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 2, aarch64::qv_2d, aarch64::qv_4s);
	cases.add_case(0x4fa32841u, "smlal2\tv1.2d, v2.4s, v3.s[3]", IB(make_smlal2_qqql), aarch64::q1,
	               aarch64::q2, aarch64::q3, 3, aarch64::qv_2d, aarch64::qv_4s);

	return cases.validate();
}
