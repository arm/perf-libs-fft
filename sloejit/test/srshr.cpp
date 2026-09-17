/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("srshr");

	cases.add_case(0x0f1f2441u, "srshr\tv1.4h, v2.4h, #1", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 1,
	               aarch64::qv_4h);
	cases.add_case(0x0f102441u, "srshr\tv1.4h, v2.4h, #16", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 16,
	               aarch64::qv_4h);
	cases.add_case(0x4f1f2441u, "srshr\tv1.8h, v2.8h, #1", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 1,
	               aarch64::qv_8h);
	cases.add_case(0x4f102441u, "srshr\tv1.8h, v2.8h, #16", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 16,
	               aarch64::qv_8h);
	cases.add_case(0x0f3f2441u, "srshr\tv1.2s, v2.2s, #1", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 1,
	               aarch64::qv_2s);
	cases.add_case(0x0f202441u, "srshr\tv1.2s, v2.2s, #32", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 32,
	               aarch64::qv_2s);
	cases.add_case(0x4f3f2441u, "srshr\tv1.4s, v2.4s, #1", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 1,
	               aarch64::qv_4s);
	cases.add_case(0x4f202441u, "srshr\tv1.4s, v2.4s, #32", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 32,
	               aarch64::qv_4s);
	cases.add_case(0x4f7f2441u, "srshr\tv1.2d, v2.2d, #1", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 1,
	               aarch64::qv_2d);
	cases.add_case(0x4f402441u, "srshr\tv1.2d, v2.2d, #64", IB(make_srshr_qqi), aarch64::q1, aarch64::q2, 64,
	               aarch64::qv_2d);
	cases.add_case(0x040c81e1u, "srshr\tz1.b, p0/m, z1.b, #1", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               1, aarch64::zv_b);
	cases.add_case(0x040c8101u, "srshr\tz1.b, p0/m, z1.b, #8", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               8, aarch64::zv_b);
	cases.add_case(0x040c83e1u, "srshr\tz1.h, p0/m, z1.h, #1", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               1, aarch64::zv_h);
	cases.add_case(0x040c8201u, "srshr\tz1.h, p0/m, z1.h, #16", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               16, aarch64::zv_h);
	cases.add_case(0x044c83e1u, "srshr\tz1.s, p0/m, z1.s, #1", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               1, aarch64::zv_s);
	cases.add_case(0x044c8001u, "srshr\tz1.s, p0/m, z1.s, #32", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               32, aarch64::zv_s);
	cases.add_case(0x04cc83e1u, "srshr\tz1.d, p0/m, z1.d, #1", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               1, aarch64::zv_d);
	cases.add_case(0x048c8001u, "srshr\tz1.d, p0/m, z1.d, #64", IB(make_srshr_zpi), aarch64::z1, aarch64::p0,
	               64, aarch64::zv_d);

	return cases.validate();
}
