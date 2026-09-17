/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("fmov");

	cases.add_case(0x1ee70041u, "fmov\th1, w2", IBO(make_fmov_hr, reg, reg), aarch64::h1, aarch64::x2);
	cases.add_case(0x1e270041u, "fmov\ts1, w2", IBO(make_fmov_sr, reg, reg), aarch64::s1, aarch64::x2);
	cases.add_case(0x9e670041u, "fmov\td1, x2", IBO(make_fmov_dr, reg, reg), aarch64::d1, aarch64::x2);
	cases.add_case(0x1ee60041u, "fmov\tw1, h2", IBO(make_fmov_rh, reg, reg), aarch64::x1, aarch64::h2);
	cases.add_case(0x1e260041u, "fmov\tw1, s2", IBO(make_fmov_rs, reg, reg), aarch64::x1, aarch64::s2);
	cases.add_case(0x9e660041u, "fmov\tx1, d2", IBO(make_fmov_rd, reg, reg), aarch64::x1, aarch64::d2);
	cases.add_case(0x1ee703e1u, "fmov\th1, wzr", IBO(make_fmov_hr, reg, reg), aarch64::h1, aarch64::xzr);
	cases.add_case(0x1e2703e1u, "fmov\ts1, wzr", IBO(make_fmov_sr, reg, reg), aarch64::s1, aarch64::xzr);
	cases.add_case(0x9e6703e1u, "fmov\td1, xzr", IBO(make_fmov_dr, reg, reg), aarch64::d1, aarch64::xzr);
	cases.add_case(0x1ee6005fu, "fmov\twzr, h2", IBO(make_fmov_rh, reg, reg), aarch64::xzr, aarch64::h2);
	cases.add_case(0x1e26005fu, "fmov\twzr, s2", IBO(make_fmov_rs, reg, reg), aarch64::xzr, aarch64::s2);
	cases.add_case(0x9e66005fu, "fmov\txzr, d2", IBO(make_fmov_rd, reg, reg), aarch64::xzr, aarch64::d2);

	return cases.validate();
}
