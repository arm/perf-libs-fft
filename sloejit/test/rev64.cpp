/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("rev64");

	cases.add_case(0x0e200841u, "rev64\tv1.8b, v2.8b", IB(make_rev64_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_8b);
	cases.add_case(0x4e200841u, "rev64\tv1.16b, v2.16b", IB(make_rev64_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_16b);
	cases.add_case(0x0e600841u, "rev64\tv1.4h, v2.4h", IB(make_rev64_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4h);
	cases.add_case(0x4e600841u, "rev64\tv1.8h, v2.8h", IB(make_rev64_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_8h);
	cases.add_case(0x0ea00841u, "rev64\tv1.2s, v2.2s", IB(make_rev64_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_2s);
	cases.add_case(0x4ea00841u, "rev64\tv1.4s, v2.4s", IB(make_rev64_qq), aarch64::q1, aarch64::q2,
	               aarch64::qv_4s);

	return cases.validate();
}
