/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

int main() {
	SloejitInstrTest cases("smlslb");

	cases.add_case(0x44435041u, "smlslb\tz1.h, z2.b, z3.b", IB(make_smlslb_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_h, aarch64::zv_b);
	cases.add_case(0x44835041u, "smlslb\tz1.s, z2.h, z3.h", IB(make_smlslb_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44c35041u, "smlslb\tz1.d, z2.s, z3.s", IB(make_smlslb_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d, aarch64::zv_s);

	cases.add_case(0x44a2a020u, "smlslb\tz0.s, z1.h, z2.h[0]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 0, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44a2a820u, "smlslb\tz0.s, z1.h, z2.h[1]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 1, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44aaa020u, "smlslb\tz0.s, z1.h, z2.h[2]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 2, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44aaa820u, "smlslb\tz0.s, z1.h, z2.h[3]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 3, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44bfa020u, "smlslb\tz0.s, z1.h, z7.h[6]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z7, 6, aarch64::zv_s, aarch64::zv_h);
	cases.add_case(0x44bfa820u, "smlslb\tz0.s, z1.h, z7.h[7]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z7, 7, aarch64::zv_s, aarch64::zv_h);

	cases.add_case(0x44e2a020u, "smlslb\tz0.d, z1.s, z2.s[0]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 0, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44e2a820u, "smlslb\tz0.d, z1.s, z2.s[1]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 1, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44f2a020u, "smlslb\tz0.d, z1.s, z2.s[2]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 2, aarch64::zv_d, aarch64::zv_s);
	cases.add_case(0x44f2a820u, "smlslb\tz0.d, z1.s, z2.s[3]", IB(make_smlslb_zzzl), aarch64::z0, aarch64::z1,
	               aarch64::z2, 3, aarch64::zv_d, aarch64::zv_s);

	return cases.validate();
}
