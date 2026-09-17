/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fcvt");

	cases.add_case(0x1ee24041u, "fcvt\ts1, h2", IBO(make_fcvt_sh, reg, reg), aarch64::s1, aarch64::h2);
	cases.add_case(0x1ee2c041u, "fcvt\td1, h2", IBO(make_fcvt_dh, reg, reg), aarch64::d1, aarch64::h2);
	cases.add_case(0x1e23c041u, "fcvt\th1, s2", IBO(make_fcvt_hs, reg, reg), aarch64::h1, aarch64::s2);
	cases.add_case(0x1e22c041u, "fcvt\td1, s2", IBO(make_fcvt_ds, reg, reg), aarch64::d1, aarch64::s2);
	cases.add_case(0x1e63c041u, "fcvt\th1, d2", IBO(make_fcvt_hd, reg, reg), aarch64::h1, aarch64::d2);
	cases.add_case(0x1e624041u, "fcvt\ts1, d2", IBO(make_fcvt_sd, reg, reg), aarch64::s1, aarch64::d2);

	return cases.validate();
}
