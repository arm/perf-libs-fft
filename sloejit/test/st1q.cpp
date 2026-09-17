/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("st1q");

	cases.add_case(0xe1ff4be1u, "st1q\t{za1h.q[w14, 0]}, p2, [sp]", IB(make_st1q_alorlpr), aarch64::za, 1,
	               aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::sp);
	cases.add_case(0xe1e24be1u, "st1q\t{za1h.q[w14, 0]}, p2, [sp, x2, lsl #4]", IB(make_st1q_alorlprr),
	               aarch64::za, 1, aarch64::hvopt_h, aarch64::x14, 0, aarch64::p2, aarch64::sp, aarch64::x2);
	cases.add_case(0xe1ffb4a9u, "st1q\t{za9v.q[w13, 0]}, p5, [x5]", IB(make_st1q_alorlprr), aarch64::za, 9,
	               aarch64::hvopt_v, aarch64::x13, 0, aarch64::p5, aarch64::x5, aarch64::xzr);
	cases.add_case(0xe1e781a0u, "st1q\t{za0v.q[w12, 0]}, p0, [x13, x7, lsl #4]", IB(make_st1q_alorlprr),
	               aarch64::za, 0, aarch64::hvopt_v, aarch64::x12, 0, aarch64::p0, aarch64::x13, aarch64::x7);

	return cases.validate();
}
