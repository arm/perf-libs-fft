/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("sqsub");

	cases.add_case(0x0e632c41u, "sqsub\tv1.4h, v2.4h, v3.4h", IB(make_sqsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4h);
	cases.add_case(0x4e632c41u, "sqsub\tv1.8h, v2.8h, v3.8h", IB(make_sqsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_8h);
	cases.add_case(0x0ea32c41u, "sqsub\tv1.2s, v2.2s, v3.2s", IB(make_sqsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2s);
	cases.add_case(0x4ea32c41u, "sqsub\tv1.4s, v2.4s, v3.4s", IB(make_sqsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_4s);
	cases.add_case(0x4ee32c41u, "sqsub\tv1.2d, v2.2d, v3.2d", IB(make_sqsub_qqq), aarch64::q1, aarch64::q2,
	               aarch64::q3, aarch64::qv_2d);
	cases.add_case(0x04231841u, "sqsub\tz1.b, z2.b, z3.b", IB(make_sqsub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_b);
	cases.add_case(0x04611862u, "sqsub\tz2.h, z3.h, z1.h", IB(make_sqsub_zzz), aarch64::z2, aarch64::z3,
	               aarch64::z1, aarch64::zv_h);
	cases.add_case(0x04a21823u, "sqsub\tz3.s, z1.s, z2.s", IB(make_sqsub_zzz), aarch64::z3, aarch64::z1,
	               aarch64::z2, aarch64::zv_s);
	cases.add_case(0x04e31841u, "sqsub\tz1.d, z2.d, z3.d", IB(make_sqsub_zzz), aarch64::z1, aarch64::z2,
	               aarch64::z3, aarch64::zv_d);

	return cases.validate();
}
