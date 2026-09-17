/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("cntp");

	cases.add_case(0x25e0a469u, "cntp\tx9, p9, p3.d", IB(make_cntp_rpp), aarch64::x9, aarch64::p9,
	               aarch64::p3, aarch64::zv_d);
	cases.add_case(0x25208000u, "cntp\tx0, p0, p0.b", IB(make_cntp_rpp), aarch64::x0, aarch64::p0,
	               aarch64::p0, aarch64::zv_b);
	cases.add_case(0x25e0847fu, "cntp\txzr, p1, p3.d", IB(make_cntp_rpp), aarch64::xzr, aarch64::p1,
	               aarch64::p3, aarch64::zv_d);

	return cases.validate();
}
