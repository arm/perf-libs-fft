/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::ptrue_pat;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ptrue");

	cases.add_case(0x2518e3e1u, "ptrue\tp1.b", IBO(make_ptrue, reg, z_type_variant), aarch64::p1,
	               aarch64::zv_b);
	cases.add_case(0x2518e3efu, "ptrue\tp15.b", IBO(make_ptrue, reg, z_type_variant), aarch64::p15,
	               aarch64::zv_b);
	cases.add_case(0x2558e3e1u, "ptrue\tp1.h", IBO(make_ptrue, reg, z_type_variant), aarch64::p1,
	               aarch64::zv_h);
	cases.add_case(0x2558e3efu, "ptrue\tp15.h", IBO(make_ptrue, reg, z_type_variant), aarch64::p15,
	               aarch64::zv_h);
	cases.add_case(0x2598e3e1u, "ptrue\tp1.s", IBO(make_ptrue, reg, z_type_variant), aarch64::p1,
	               aarch64::zv_s);
	cases.add_case(0x2598e3efu, "ptrue\tp15.s", IBO(make_ptrue, reg, z_type_variant), aarch64::p15,
	               aarch64::zv_s);
	cases.add_case(0x25d8e3e1u, "ptrue\tp1.d", IBO(make_ptrue, reg, z_type_variant), aarch64::p1,
	               aarch64::zv_d);
	cases.add_case(0x25d8e3efu, "ptrue\tp15.d", IBO(make_ptrue, reg, z_type_variant), aarch64::p15,
	               aarch64::zv_d);
	cases.add_case(0x2518e000u, "ptrue\tp0.b, pow2", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p0, aarch64::ptrue_pat_pow2, aarch64::zv_b);
	cases.add_case(0x2518e021u, "ptrue\tp1.b, vl1", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p1, aarch64::ptrue_pat_vl1, aarch64::zv_b);
	cases.add_case(0x2518e042u, "ptrue\tp2.b, vl2", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p2, aarch64::ptrue_pat_vl2, aarch64::zv_b);
	cases.add_case(0x2518e063u, "ptrue\tp3.b, vl3", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p3, aarch64::ptrue_pat_vl3, aarch64::zv_b);
	cases.add_case(0x2518e084u, "ptrue\tp4.b, vl4", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p4, aarch64::ptrue_pat_vl4, aarch64::zv_b);
	cases.add_case(0x2518e0a5u, "ptrue\tp5.b, vl5", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p5, aarch64::ptrue_pat_vl5, aarch64::zv_b);
	cases.add_case(0x2518e0c6u, "ptrue\tp6.b, vl6", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p6, aarch64::ptrue_pat_vl6, aarch64::zv_b);
	cases.add_case(0x2518e0e7u, "ptrue\tp7.b, vl7", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p7, aarch64::ptrue_pat_vl7, aarch64::zv_b);
	cases.add_case(0x2518e108u, "ptrue\tp8.b, vl8", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p8, aarch64::ptrue_pat_vl8, aarch64::zv_b);
	cases.add_case(0x2518e129u, "ptrue\tp9.b, vl16", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p9, aarch64::ptrue_pat_vl16, aarch64::zv_b);
	cases.add_case(0x2518e14au, "ptrue\tp10.b, vl32", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p10, aarch64::ptrue_pat_vl32, aarch64::zv_b);
	cases.add_case(0x2518e16bu, "ptrue\tp11.b, vl64", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p11, aarch64::ptrue_pat_vl64, aarch64::zv_b);
	cases.add_case(0x2518e18cu, "ptrue\tp12.b, vl128", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p12, aarch64::ptrue_pat_vl128, aarch64::zv_b);
	cases.add_case(0x2518e1adu, "ptrue\tp13.b, vl256", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p13, aarch64::ptrue_pat_vl256, aarch64::zv_b);
	cases.add_case(0x2518e3aeu, "ptrue\tp14.b, mul4", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p14, aarch64::ptrue_pat_mul4, aarch64::zv_b);
	cases.add_case(0x2518e3cfu, "ptrue\tp15.b, mul3", IBO(make_ptrue, reg, ptrue_pat, z_type_variant),
	               aarch64::p15, aarch64::ptrue_pat_mul3, aarch64::zv_b);

	return cases.validate();
}
