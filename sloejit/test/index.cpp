/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "instr_test.hpp"

using reg = sloejit::reg;
using aarch64::z_type_variant;

int main() {
	SloejitInstrTest cases("index");

	cases.add_case(0x04204000u, "index\tz0.b, #0, #0", IB(make_index_zii), aarch64::z0, 0, 0, aarch64::zv_b);
	cases.add_case(0x042f421fu, "index\tz31.b, #-16, #15", IB(make_index_zii), aarch64::z31, -16, 15,
	               aarch64::zv_b);
	cases.add_case(0x043041ffu, "index\tz31.b, #15, #-16", IB(make_index_zii), aarch64::z31, 15, -16,
	               aarch64::zv_b);
	cases.add_case(0x04604000u, "index\tz0.h, #0, #0", IB(make_index_zii), aarch64::z0, 0, 0, aarch64::zv_h);
	cases.add_case(0x046f421fu, "index\tz31.h, #-16, #15", IB(make_index_zii), aarch64::z31, -16, 15,
	               aarch64::zv_h);
	cases.add_case(0x047041ffu, "index\tz31.h, #15, #-16", IB(make_index_zii), aarch64::z31, 15, -16,
	               aarch64::zv_h);
	cases.add_case(0x04a04000u, "index\tz0.s, #0, #0", IB(make_index_zii), aarch64::z0, 0, 0, aarch64::zv_s);
	cases.add_case(0x04af421fu, "index\tz31.s, #-16, #15", IB(make_index_zii), aarch64::z31, -16, 15,
	               aarch64::zv_s);
	cases.add_case(0x04b041ffu, "index\tz31.s, #15, #-16", IB(make_index_zii), aarch64::z31, 15, -16,
	               aarch64::zv_s);
	cases.add_case(0x04e04000u, "index\tz0.d, #0, #0", IB(make_index_zii), aarch64::z0, 0, 0, aarch64::zv_d);
	cases.add_case(0x04ef421fu, "index\tz31.d, #-16, #15", IB(make_index_zii), aarch64::z31, -16, 15,
	               aarch64::zv_d);
	cases.add_case(0x04f041ffu, "index\tz31.d, #15, #-16", IB(make_index_zii), aarch64::z31, 15, -16,
	               aarch64::zv_d);
	cases.add_case(0x04214800u, "index\tz0.b, #0, w1", IB(make_index_zir), aarch64::z0, 0, aarch64::x1,
	               aarch64::zv_b);
	cases.add_case(0x043f4a1fu, "index\tz31.b, #-16, wzr", IB(make_index_zir), aarch64::z31, -16,
	               aarch64::xzr, aarch64::zv_b);
	cases.add_case(0x043f49ffu, "index\tz31.b, #15, wzr", IB(make_index_zir), aarch64::z31, 15, aarch64::xzr,
	               aarch64::zv_b);
	cases.add_case(0x04614800u, "index\tz0.h, #0, w1", IB(make_index_zir), aarch64::z0, 0, aarch64::x1,
	               aarch64::zv_h);
	cases.add_case(0x047f4a1fu, "index\tz31.h, #-16, wzr", IB(make_index_zir), aarch64::z31, -16,
	               aarch64::xzr, aarch64::zv_h);
	cases.add_case(0x047f49ffu, "index\tz31.h, #15, wzr", IB(make_index_zir), aarch64::z31, 15, aarch64::xzr,
	               aarch64::zv_h);
	cases.add_case(0x04a14800u, "index\tz0.s, #0, w1", IB(make_index_zir), aarch64::z0, 0, aarch64::x1,
	               aarch64::zv_s);
	cases.add_case(0x04bf4a1fu, "index\tz31.s, #-16, wzr", IB(make_index_zir), aarch64::z31, -16,
	               aarch64::xzr, aarch64::zv_s);
	cases.add_case(0x04bf49ffu, "index\tz31.s, #15, wzr", IB(make_index_zir), aarch64::z31, 15, aarch64::xzr,
	               aarch64::zv_s);
	cases.add_case(0x04e14800u, "index\tz0.d, #0, x1", IB(make_index_zir), aarch64::z0, 0, aarch64::x1,
	               aarch64::zv_d);
	cases.add_case(0x04ff4a1fu, "index\tz31.d, #-16, xzr", IB(make_index_zir), aarch64::z31, -16,
	               aarch64::xzr, aarch64::zv_d);
	cases.add_case(0x04ff49ffu, "index\tz31.d, #15, xzr", IB(make_index_zir), aarch64::z31, 15, aarch64::xzr,
	               aarch64::zv_d);
	cases.add_case(0x04204420u, "index\tz0.b, w1, #0", IB(make_index_zri), aarch64::z0, aarch64::x1, 0,
	               aarch64::zv_b);
	cases.add_case(0x043047ffu, "index\tz31.b, wzr, #-16", IB(make_index_zri), aarch64::z31, aarch64::xzr,
	               -16, aarch64::zv_b);
	cases.add_case(0x042f47ffu, "index\tz31.b, wzr, #15", IB(make_index_zri), aarch64::z31, aarch64::xzr, 15,
	               aarch64::zv_b);
	cases.add_case(0x04604420u, "index\tz0.h, w1, #0", IB(make_index_zri), aarch64::z0, aarch64::x1, 0,
	               aarch64::zv_h);
	cases.add_case(0x047047ffu, "index\tz31.h, wzr, #-16", IB(make_index_zri), aarch64::z31, aarch64::xzr,
	               -16, aarch64::zv_h);
	cases.add_case(0x046f47ffu, "index\tz31.h, wzr, #15", IB(make_index_zri), aarch64::z31, aarch64::xzr, 15,
	               aarch64::zv_h);
	cases.add_case(0x04a04420u, "index\tz0.s, w1, #0", IB(make_index_zri), aarch64::z0, aarch64::x1, 0,
	               aarch64::zv_s);
	cases.add_case(0x04b047ffu, "index\tz31.s, wzr, #-16", IB(make_index_zri), aarch64::z31, aarch64::xzr,
	               -16, aarch64::zv_s);
	cases.add_case(0x04af47ffu, "index\tz31.s, wzr, #15", IB(make_index_zri), aarch64::z31, aarch64::xzr, 15,
	               aarch64::zv_s);
	cases.add_case(0x04e04420u, "index\tz0.d, x1, #0", IB(make_index_zri), aarch64::z0, aarch64::x1, 0,
	               aarch64::zv_d);
	cases.add_case(0x04f047ffu, "index\tz31.d, xzr, #-16", IB(make_index_zri), aarch64::z31, aarch64::xzr,
	               -16, aarch64::zv_d);
	cases.add_case(0x04ef47ffu, "index\tz31.d, xzr, #15", IB(make_index_zri), aarch64::z31, aarch64::xzr, 15,
	               aarch64::zv_d);
	cases.add_case(0x04224c20u, "index\tz0.b, w1, w2", IB(make_index_zrr), aarch64::z0, aarch64::x1,
	               aarch64::x2, aarch64::zv_b);
	cases.add_case(0x043f4fffu, "index\tz31.b, wzr, wzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_b);
	cases.add_case(0x043f4fffu, "index\tz31.b, wzr, wzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_b);
	cases.add_case(0x04624c20u, "index\tz0.h, w1, w2", IB(make_index_zrr), aarch64::z0, aarch64::x1,
	               aarch64::x2, aarch64::zv_h);
	cases.add_case(0x047f4fffu, "index\tz31.h, wzr, wzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_h);
	cases.add_case(0x047f4fffu, "index\tz31.h, wzr, wzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_h);
	cases.add_case(0x04a24c20u, "index\tz0.s, w1, w2", IB(make_index_zrr), aarch64::z0, aarch64::x1,
	               aarch64::x2, aarch64::zv_s);
	cases.add_case(0x04bf4fffu, "index\tz31.s, wzr, wzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_s);
	cases.add_case(0x04bf4fffu, "index\tz31.s, wzr, wzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_s);
	cases.add_case(0x04e24c20u, "index\tz0.d, x1, x2", IB(make_index_zrr), aarch64::z0, aarch64::x1,
	               aarch64::x2, aarch64::zv_d);
	cases.add_case(0x04ff4fffu, "index\tz31.d, xzr, xzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_d);
	cases.add_case(0x04ff4fffu, "index\tz31.d, xzr, xzr", IB(make_index_zrr), aarch64::z31, aarch64::xzr,
	               aarch64::xzr, aarch64::zv_d);

	return cases.validate();
}
