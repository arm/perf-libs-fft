/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("pfalse");

	cases.add_case(0x2518e401u, "pfalse\tp1.b", IBO(make_pfalse, reg), aarch64::p1);
	cases.add_case(0x2518e40fu, "pfalse\tp15.b", IBO(make_pfalse, reg), aarch64::p15);

	return cases.validate();
}
