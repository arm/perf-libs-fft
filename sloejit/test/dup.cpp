/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("dup");

	cases.add_case(0x0e010441u, "dup\tv1.8b, v2.b[0]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 0,
	               aarch64::qv_8b);
	cases.add_case(0x0e1f0441u, "dup\tv1.8b, v2.b[15]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 15,
	               aarch64::qv_8b);
	cases.add_case(0x4e1f07e1u, "dup\tv1.16b, v31.b[15]", IB(make_dup_qql), aarch64::q1, aarch64::q31, 15,
	               aarch64::qv_16b);
	cases.add_case(0x0e1e0441u, "dup\tv1.4h, v2.h[7]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 7,
	               aarch64::qv_4h);
	cases.add_case(0x4e1e0441u, "dup\tv1.8h, v2.h[7]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 7,
	               aarch64::qv_8h);
	cases.add_case(0x0e1c0441u, "dup\tv1.2s, v2.s[3]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 3,
	               aarch64::qv_2s);
	cases.add_case(0x4e1c0441u, "dup\tv1.4s, v2.s[3]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 3,
	               aarch64::qv_4s);
	cases.add_case(0x4e180441u, "dup\tv1.2d, v2.d[1]", IB(make_dup_qql), aarch64::q1, aarch64::q2, 1,
	               aarch64::qv_2d);
	cases.add_case(0x0e010fe1u, "dup\tv1.8b, wzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_8b);
	cases.add_case(0x4e010fe1u, "dup\tv1.16b, wzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_16b);
	cases.add_case(0x0e020fe1u, "dup\tv1.4h, wzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_4h);
	cases.add_case(0x4e020fe1u, "dup\tv1.8h, wzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_8h);
	cases.add_case(0x0e040fe1u, "dup\tv1.2s, wzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_2s);
	cases.add_case(0x4e040fe1u, "dup\tv1.4s, wzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_4s);
	cases.add_case(0x4e080fe1u, "dup\tv1.2d, xzr", IB(make_dup_qr), aarch64::q1, aarch64::xzr,
	               aarch64::qv_2d);
	cases.add_case(0x2538c001u, "dup\tz1.b, #0", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1, 0,
	               aarch64::zv_b);
	cases.add_case(0x2538d001u, "dup\tz1.b, #-128", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               -128, aarch64::zv_b);
	cases.add_case(0x2538cfe1u, "dup\tz1.b, #127", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               127, aarch64::zv_b);
	cases.add_case(0x2578c001u, "dup\tz1.h, #0", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1, 0,
	               aarch64::zv_h);
	cases.add_case(0x2578d001u, "dup\tz1.h, #-128", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               -128, aarch64::zv_h);
	cases.add_case(0x2578cfe1u, "dup\tz1.h, #127", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               127, aarch64::zv_h);
	cases.add_case(0x25b8c001u, "dup\tz1.s, #0", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1, 0,
	               aarch64::zv_s);
	cases.add_case(0x25b8d001u, "dup\tz1.s, #-128", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               -128, aarch64::zv_s);
	cases.add_case(0x25b8cfe1u, "dup\tz1.s, #127", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               127, aarch64::zv_s);
	cases.add_case(0x25f8c001u, "dup\tz1.d, #0", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1, 0,
	               aarch64::zv_d);
	cases.add_case(0x25f8d001u, "dup\tz1.d, #-128", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               -128, aarch64::zv_d);
	cases.add_case(0x25f8cfe1u, "dup\tz1.d, #127", IBO(make_dup_zi, reg, int, z_type_variant), aarch64::z1,
	               127, aarch64::zv_d);
	cases.add_case(0x05212041u, "dup\tz1.b, z2.b[0]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 0,
	               aarch64::zv_b);
	cases.add_case(0x05ff2041u, "dup\tz1.b, z2.b[63]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 63,
	               aarch64::zv_b);
	cases.add_case(0x05222041u, "dup\tz1.h, z2.h[0]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 0,
	               aarch64::zv_h);
	cases.add_case(0x05fe2041u, "dup\tz1.h, z2.h[31]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 31,
	               aarch64::zv_h);
	cases.add_case(0x05242041u, "dup\tz1.s, z2.s[0]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 0,
	               aarch64::zv_s);
	cases.add_case(0x05fc2041u, "dup\tz1.s, z2.s[15]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 15,
	               aarch64::zv_s);
	cases.add_case(0x05282041u, "dup\tz1.d, z2.d[0]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 0,
	               aarch64::zv_d);
	cases.add_case(0x05f82041u, "dup\tz1.d, z2.d[7]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 7,
	               aarch64::zv_d);
	cases.add_case(0x05302041u, "dup\tz1.q, z2.q[0]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 0,
	               aarch64::zv_q);
	cases.add_case(0x05f02041u, "dup\tz1.q, z2.q[3]", IB(make_dup_zzl), aarch64::z1, aarch64::z2, 3,
	               aarch64::zv_q);

	return cases.validate();
}
