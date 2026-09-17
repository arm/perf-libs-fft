/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("movz");

	cases.add_case(0xd280001fu, "movz\txzr, #0", IB(make_x_movz_ri), aarch64::xzr, 0u);
	cases.add_case(0xd29fffe1u, "movz\tx1, #65535", IB(make_x_movz_ri), aarch64::x1, 65535u);
	cases.add_case(0xd2bfffe2u, "movz\tx2, #65535, lsl #16", IB(make_x_movz_ri), aarch64::x2,
	               65535ull << 16ull);
	cases.add_case(0xd2dfffe3u, "movz\tx3, #65535, lsl #32", IB(make_x_movz_ri), aarch64::x3,
	               65535ull << 32ull);
	cases.add_case(0xd2ffffe4u, "movz\tx4, #65535, lsl #48", IB(make_x_movz_ri), aarch64::x4,
	               65535ull << 48ull);

	return cases.validate();
}
