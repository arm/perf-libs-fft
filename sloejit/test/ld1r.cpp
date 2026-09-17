/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("ld1r");

	cases.add_case(0x0d40c041u, "ld1r\t{v1.8b}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_8b);
	cases.add_case(0x0d40c3e1u, "ld1r\t{v1.8b}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_8b);
	cases.add_case(0x4d40c041u, "ld1r\t{v1.16b}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_16b);
	cases.add_case(0x4d40c3e1u, "ld1r\t{v1.16b}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_16b);
	cases.add_case(0x0d40c441u, "ld1r\t{v1.4h}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_4h);
	cases.add_case(0x0d40c7e1u, "ld1r\t{v1.4h}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_4h);
	cases.add_case(0x4d40c441u, "ld1r\t{v1.8h}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_8h);
	cases.add_case(0x4d40c7e1u, "ld1r\t{v1.8h}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_8h);
	cases.add_case(0x0d40c841u, "ld1r\t{v1.2s}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_2s);
	cases.add_case(0x0d40cbe1u, "ld1r\t{v1.2s}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_2s);
	cases.add_case(0x4d40c841u, "ld1r\t{v1.4s}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_4s);
	cases.add_case(0x4d40cbe1u, "ld1r\t{v1.4s}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_4s);
	cases.add_case(0x4d40cc41u, "ld1r\t{v1.2d}, [x2]", IB(make_ld1r_rr), aarch64::q1, aarch64::x2,
	               aarch64::q_type_variant::qv_2d);
	cases.add_case(0x4d40cfe1u, "ld1r\t{v1.2d}, [sp]", IB(make_ld1r_rr), aarch64::q1, aarch64::sp,
	               aarch64::q_type_variant::qv_2d);

	return cases.validate();
}
