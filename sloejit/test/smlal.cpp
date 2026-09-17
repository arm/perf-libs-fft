/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("smlal");

	cases.add_case(0x0e238041u, "smlal\tv1.8h, v2.8b, v3.8b", IB(make_smlal_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h, aarch64::qv_8b);
	cases.add_case(0x0e638041u, "smlal\tv1.4s, v2.4h, v3.4h", IB(make_smlal_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0ea38041u, "smlal\tv1.2d, v2.2s, v3.2s", IB(make_smlal_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d, aarch64::qv_2s);

	cases.add_case(0x0f432041u, "smlal\tv1.4s, v2.4h, v3.h[0]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f532041u, "smlal\tv1.4s, v2.4h, v3.h[1]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f632041u, "smlal\tv1.4s, v2.4h, v3.h[2]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 2, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f732041u, "smlal\tv1.4s, v2.4h, v3.h[3]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f432841u, "smlal\tv1.4s, v2.4h, v3.h[4]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 4, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f532841u, "smlal\tv1.4s, v2.4h, v3.h[5]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 5, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f632841u, "smlal\tv1.4s, v2.4h, v3.h[6]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 6, aarch64::qv_4s, aarch64::qv_4h);
	cases.add_case(0x0f732841u, "smlal\tv1.4s, v2.4h, v3.h[7]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 7, aarch64::qv_4s, aarch64::qv_4h);

	cases.add_case(0x0f832041u, "smlal\tv1.2d, v2.2s, v3.s[0]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 0, aarch64::qv_2d, aarch64::qv_2s);
	cases.add_case(0x0fa32041u, "smlal\tv1.2d, v2.2s, v3.s[1]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 1, aarch64::qv_2d, aarch64::qv_2s);
	cases.add_case(0x0f832841u, "smlal\tv1.2d, v2.2s, v3.s[2]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 2, aarch64::qv_2d, aarch64::qv_2s);
	cases.add_case(0x0fa32841u, "smlal\tv1.2d, v2.2s, v3.s[3]", IB(make_smlal_qqql), aarch64::q1, aarch64::q2,
	               aarch64::q3, 3, aarch64::qv_2d, aarch64::qv_2s);

	return cases.validate();
}
