/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("ld1q");

	cases.add_case(0xe1df0000u, "ld1q\t{za0h.q[w12, 0]}, p0/z, [x0]", IB(make_ld1q_alorlpr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0);
	cases.add_case(0xe1df0000u, "ld1q\t{za0h.q[w12, 0]}, p0/z, [x0]", IB(make_ld1q_alorlprr), aarch64::za, 0,
	               aarch64::hvopt_h, aarch64::x12, 0, aarch64::p0, aarch64::x0, aarch64::xzr);
	cases.add_case(0xe1c1afefu, "ld1q\t{za15v.q[w13, 0]}, p3/z, [sp, x1, lsl #4]", IB(make_ld1q_alorlprr),
	               aarch64::za, 15, aarch64::hvopt_v, aarch64::x13, 0, aarch64::p3, aarch64::sp, aarch64::x1);
	cases.add_case(0xe1cd7ce5u, "ld1q\t{za5h.q[w15, 0]}, p7/z, [x7, x13, lsl #4]", IB(make_ld1q_alorlprr),
	               aarch64::za, 5, aarch64::hvopt_h, aarch64::x15, 0, aarch64::p7, aarch64::x7, aarch64::x13);

	return cases.validate();
}
