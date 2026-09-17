/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("smullb");

	cases.add_case(0x45437041u, "smullb	z1.h, z2.b, z3.b", IB(make_smullb_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h, aarch64::zv_b);
	cases.add_case(0x45837041u, "smullb	z1.s, z2.h, z3.h", IB(make_smullb_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x45c37041u, "smullb	z1.d, z2.s, z3.s", IB(make_smullb_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d, aarch64::zv_s);

	cases.add_case(0x44a2c020u, "smullb	z0.s, z1.h, z2.h[0]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 0, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44a2c820u, "smullb	z0.s, z1.h, z2.h[1]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 1, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44aac020u, "smullb	z0.s, z1.h, z2.h[2]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 2, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44aac820u, "smullb	z0.s, z1.h, z2.h[3]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 3, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44bfc020u, "smullb	z0.s, z1.h, z7.h[6]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z7, 6, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44bfc820u, "smullb	z0.s, z1.h, z7.h[7]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z7, 7, aarch64::zv_s, aarch64::zv_h);

	cases.add_case(0x44e2c020u, "smullb	z0.d, z1.s, z2.s[0]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 0, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44e2c820u, "smullb	z0.d, z1.s, z2.s[1]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 1, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44f2c020u, "smullb	z0.d, z1.s, z2.s[2]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 2, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44f2c820u, "smullb	z0.d, z1.s, z2.s[3]", IB(make_smullb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 3, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44efc1cfu, "smullb	z15.d, z14.s, z15.s[0]", IB(make_smullb_zzzl), aarch64::z15,
	               aarch64::z14, aarch64::z15, 0, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44ffc9cfu, "smullb	z15.d, z14.s, z15.s[3]", IB(make_smullb_zzzl), aarch64::z15,
	               aarch64::z14, aarch64::z15, 3, aarch64::zv_d, aarch64::zv_s);

	return cases.validate();
}
