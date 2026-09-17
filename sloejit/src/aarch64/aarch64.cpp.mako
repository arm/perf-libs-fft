/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

<% opcodes = load_module('../include/sloejit/aarch64/aarch64_opcodes.py').opcodes %>\
#include "sloejit/aarch64/aarch64.hpp"
#include "sloejit/aarch64/aarch64_opcodes.hpp"
#include "sloejit_assert.hpp"

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#define R_AARCH64_ADD_ABS_LO12_NC 277
#define R_AARCH64_ADR_PREL_LO21 274
#define R_AARCH64_ADR_PREL_PG_HI21 275
#define R_AARCH64_CALL26 283

namespace aarch64 = sloejit::aarch64;

using block = sloejit::block;
using branch_target = sloejit::branch_target;
using bytevector = sloejit::bytevector;
using function = sloejit::function;
using function_options_t = sloejit::function_options_t;
using instr_base = sloejit::instr_base;
using instruction = sloejit::instruction;
using live_position_elem = sloejit::live_position_elem;
using live_positions = sloejit::live_positions;
using live_range = sloejit::live_range;
using reg = sloejit::reg;
using regset = sloejit::regset;
using regset_one_space = sloejit::regset_one_space;
using reloc_info = sloejit::reloc_info;

static uint8_t reg_get_active_mask(reg r) {
	sloejit_assert(r.space_id > 0);
	sloejit_assert(r.id > 0);
	sloejit_assert(r.active_mask > 0);
	return r.active_mask;
}

template <typename... Args>
inline uint64_t reg_assert_classes_equal_and_get(reg r, Args... rs) {
	auto rc = reg_get_active_mask(r);
	(sloejit_assert(reg_get_active_mask(rs) == rc), ...);
	return rc;
}

template <typename... Args>
inline void reg_assert_classes_equal_to(aarch64::preg_classes rc, Args... rs) {
	(sloejit_assert(reg_get_active_mask(rs) == rc), ...);
}

template <typename... Args>
inline void reg_assert_classes_equal(reg r, Args... rs) {
	auto rc = reg_get_active_mask(r);
	(sloejit_assert(reg_get_active_mask(rs) == rc), ...);
}

reg aarch64::reg_reinterpret_with_class(reg r, preg_classes active_mask) {
	return { r.space_id, r.id, active_mask };
}

static void create_bin_madd_rrrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t ra) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(ra < 32);
	// madd.xml
	uint32_t base = 0b1'0011011000'00000'0'00000'00000'00000u;
	//               sf            ~Rm~~   ~Ra~~ ~Rn~~ ~Rd~~
	//             31-^             16-^    10-^   5-^   0-^
	bv.push_u32(base | (rm << 16) | (ra << 10) | (rn << 5) | rd);
}

template <uint32_t S>
static void create_bin_x_sub_ri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	uint32_t sh = (imm & 0xfff000u) ? 1 : 0;
	imm = (imm & 0xfff000u) ? (imm >> 12) : imm;
	// sub_addsub_imm.xml
	// subs_addsub_imm.xml
	uint32_t base = 0b1'1'0'100010'0'000000000000'00000'00000u;
	//               sf   S       sh ~~~~imm~~~~~ ~Rn~~ ~Rd~~
	//             31-^   ^-29  22-^         10-^   5-^   0-^
	bv.push_u32(base | (S << 29) | (sh << 22) | (imm << 10) | (rn << 5) | rd);
}

static void create_bin_sub_rri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	create_bin_x_sub_ri<0>(bv, rd, rn, imm);
}

static void create_bin_subs_rri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	create_bin_x_sub_ri<1>(bv, rd, rn, imm);
}

template <uint32_t S>
static void create_bin_sub_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t shift) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(shift < 64);
	// sub_addsub_shift.xml
	// subs_addsub_shift.xml
	// note: we only care about 64-bit variants for now, hence sf=1.
	uint32_t base = 0b1'1'0'01011'00'0'00000'000000'00000'00000u;
	//               sf   S       sh   ~Rm~~ ~imm~~ ~Rn~~ ~Rd~~
	//             31-^   ^-29  22-^    16-^   10-^   5-^   0-^
	bv.push_u32(base | (S << 29) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
}

static void create_bin_sub_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t shift) {
	create_bin_sub_rrr<0>(bv, rd, rn, rm, shift);
}

static void create_bin_subs_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	// TODO: shift not exposed
	create_bin_sub_rrr<1>(bv, rd, rn, rm, 0);
}

template <uint32_t S>
static void create_bin_x_add_ri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	uint32_t sh = (imm & 0xfff000u) ? 1 : 0;
	imm = (imm & 0xfff000u) ? (imm >> 12) : imm;
	// add_addsub_imm.xml
	// adds_addsub_imm.xml
	uint32_t base = 0b1'0'0'100010'0'000000000000'00000'00000u;
	//               sf   S       sh ~~~~imm~~~~~ ~Rn~~ ~Rd~~
	//             31-^   ^-29  22-^         10-^   5-^   0-^
	bv.push_u32(base | (S << 29) | (sh << 22) | (imm << 10) | (rn << 5) | rd);
}

static void create_bin_add_rri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	create_bin_x_add_ri<0>(bv, rd, rn, imm);
}

static void create_bin_adds_rri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	create_bin_x_add_ri<1>(bv, rd, rn, imm);
}

template <uint32_t S>
static void create_bin_x_add_rr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t shift) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(shift < 64);
	// add_addsub_shift.xml
	// adds_addsub_shift.xml
	// note: we only care about 64-bit variants for now, hence sf=1.
	uint32_t base = 0b1'0'0'01011'00'0'00000'000000'00000'00000u;
	//               sf   S       sh   ~Rm~~ ~imm~~ ~Rn~~ ~Rd~~
	//             31-^   ^-29  22-^    16-^   10-^   5-^   0-^
	bv.push_u32(base | (S << 29) | (rm << 16) | (shift << 10) | (rn << 5) | rd);
}

static void create_bin_add_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t shift) {
	create_bin_x_add_rr<0>(bv, rd, rn, rm, shift);
}

static void create_bin_adds_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	// TODO: shift not exposed
	create_bin_x_add_rr<1>(bv, rd, rn, rm, 0);
}

template <uint32_t s>
static void create_bin_x_xdiv_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sdiv.xml
	// udiv.xml
	// note: we only care about 64-bit variants for now, hence sf=1.
	uint32_t base = 0b10011010110'00000'00001'0'00000'00000u;
	//                            ~Rm~~       s ~Rn~~ ~Rd~~
	//                             16-^    10-^   5-^   0-^
	bv.push_u32(base | (rm << 16) | (s << 10) | (rn << 5) | rd);
}

static void create_bin_sdiv_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_x_xdiv_rrr<1>(bv, rd, rn, rm);
}

static void create_bin_udiv_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_x_xdiv_rrr<0>(bv, rd, rn, rm);
}

template<unsigned op>
static void create_bin_sbfm_ubfm_rrii(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t immr, uint32_t imms) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(immr < 64);
	sloejit_assert(imms < 64);
	// ubfm.xml
	// sbfm.xml
	// note: we only care about 64-bit variants for now, hence sf=1 and N=1.
	uint32_t base = 0b1'00'1001101'000000'000000'00000'00000u;
	//                  op         ~immr~ ~imms~ ~Rn~~ ~Rd~~
	//                29-^           16-^   10-^   5-^   0-^
	bv.push_u32(base | (op << 29) | (immr << 16) | (imms << 10) | (rn << 5) | rd);
}

static void create_bin_lsl_rri(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm) {
	uint32_t immr = (64u - imm) % 64u;
	uint32_t imms = 63u - imm;
	create_bin_sbfm_ubfm_rrii<0b10>(bv, rd, rn, immr, imms);
}

static void create_bin_lsr_rri(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm) {
	uint32_t immr = imm;
	uint32_t imms = 63u;
	create_bin_sbfm_ubfm_rrii<0b10>(bv, rd, rn, immr, imms);
}

static void create_bin_asr_rri(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm) {
	uint32_t immr = imm;
	uint32_t imms = 63u;
	create_bin_sbfm_ubfm_rrii<0b00>(bv, rd, rn, immr, imms);
}

template <uint32_t ftype, uint32_t op>
static void create_bin_faddsub_fff(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fadd_float.xml
	// fsub_float.xml
	// fmul_float.xml
	uint32_t base = 0b00011110'00'1'00000'00'00'10'00000'00000u;
	//                         ft   ~Rm~~    op    ~Rn~~ ~Rd~~
	//                       22-^    16-^  12-^      5-^   0-^
	bv.push_u32(base | (ftype << 22) | (rm << 16) | (op << 12) | (rn << 5) | rd);
}

static void create_bin_fadd_hhh(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b11, 0b10>(bv, rd, rn, rm);
}

static void create_bin_fadd_sss(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b00, 0b10>(bv, rd, rn, rm);
}

static void create_bin_fadd_ddd(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b01, 0b10>(bv, rd, rn, rm);
}

static void create_bin_fsub_hhh(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b11, 0b11>(bv, rd, rn, rm);
}

static void create_bin_fsub_sss(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b00, 0b11>(bv, rd, rn, rm);
}

static void create_bin_fsub_ddd(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b01, 0b11>(bv, rd, rn, rm);
}

static void create_bin_fmul_hhh(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b11, 0b00>(bv, rd, rn, rm);
}

static void create_bin_fmul_sss(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b00, 0b00>(bv, rd, rn, rm);
}

static void create_bin_fmul_ddd(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	create_bin_faddsub_fff<0b01, 0b00>(bv, rd, rn, rm);
}

template <uint32_t op>
static void create_bin_faddsub_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fadd_advsimd.xml
	// fsub_advsimd.xml
	uint32_t base = 0b0'0'001110'0'00'00000'00'0101'00000'00000u;
	//                  Q       op sz ~Rm~~ ft      ~Rn~~ ~Rd~~
	//               30-^        21-^  16-^  ^-14     5-^   0-^
	uint32_t q = 0, sz = 0, ftype = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b10;
		ftype = 0b00;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b10;
		ftype = 0b00;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b01;
		ftype = 0b11;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b01;
		ftype = 0b11;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		ftype = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (op << 23) | (sz << 21) | (rm << 16) | (ftype << 14) | (rn << 5) | rd);
}

static void create_bin_fadd_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_faddsub_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_fsub_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_faddsub_qqq<1>(bv, rd, rn, rm, qv);
}

static void create_bin_fmul_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fmul_advsimd_vec.xml
	uint32_t base = 0b0'0'1011100'00'00000'00'0111'00000'00000u;
	//                  Q         sz ~Rm~~ ft      ~Rn~~ ~Rd~~
	//               30-^       21-^  16-^  ^-14     5-^   0-^
	uint32_t q = 0, sz = 0, ftype = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b10;
		ftype = 0b00;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b10;
		ftype = 0b00;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b01;
		ftype = 0b11;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b01;
		ftype = 0b11;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		ftype = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 21) | (rm << 16) | (ftype << 14) | (rn << 5) | rd);
}

static uint32_t get_zv_sz_hsd(aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_h: return 0b01u;
	case aarch64::zv_s: return 0b10u;
	case aarch64::zv_d: return 0b11u;
	default: sloejit_assert(false);
	}
	return 0;
}

static uint32_t get_zv_sz_bhsd(aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_b: return 0b00u;
	case aarch64::zv_h: return 0b01u;
	case aarch64::zv_s: return 0b10u;
	case aarch64::zv_d: return 0b11u;
	default: sloejit_assert(false);
	}
	return 0;
}

static uint32_t get_zv_sz_bhsdq(aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_b: return 0b00u;
	case aarch64::zv_h: return 0b01u;
	case aarch64::zv_s: return 0b10u;
	case aarch64::zv_d: return 0b11u;
	// Q is the same as D, but there will also be a Q bit that needs to be set
	case aarch64::zv_q: return 0b11u;
	default: sloejit_assert(false);
	}
	return 0;
}

template<uint32_t op>
static void create_bin_addsub_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// add_advsimd.xml
	// sub_advsimd.xml
	uint32_t base = 0b0'0'0'01110'00'1'00000'100001'00000'00000u;
	//                  Q op      sz   ~Rm~~        ~Rn~~ ~Rd~~
	//               30-^ ^-29  22-^    16-^          5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (op << 29) | (sz << 22) | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_add_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                              aarch64::q_type_variant qv) {
	create_bin_addsub_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_sub_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                              aarch64::q_type_variant qv) {
	create_bin_addsub_qqq<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_addsub_zi(bytevector &bv, uint32_t rdn, uint32_t imm, aarch64::z_type_variant zv) {
	sloejit_assert(rdn < 32);
	sloejit_assert((imm & 0xffu) == imm || (imm & 0xff00u) == imm);
	// add_z_zi.xml
	// sub_z_zi.xml
	uint32_t base = 0b00100101'00'10000'0'11'0'00000000'00000u;
	//                         sz      op   sh ~~imm8~~ ~Rdn~
	//                       22-^    16-^ 13-^      5-^   0-^
	uint32_t sh = (imm & 0xffu) == imm ? 0 : 1;
	imm >>= 8 * sh;
	sloejit_assert(zv != aarch64::zv_b || sh == 0); // reserved
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (op << 16) | (sh << 13) | (imm << 5) | rdn);
}

static void create_bin_add_zi(bytevector &bv, uint32_t rdn, uint32_t imm, aarch64::z_type_variant zv) {
	create_bin_addsub_zi<0>(bv, rdn, imm, zv);
}

static void create_bin_sub_zi(bytevector &bv, uint32_t rdn, uint32_t imm, aarch64::z_type_variant zv) {
	create_bin_addsub_zi<1>(bv, rdn, imm, zv);
}

template <uint32_t op>
static void create_bin_addsub_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// add_z_zz.xml
	uint32_t base = 0b00000100'00'1'00000'00000'0'00000'00000u;
	//                         sz   ~Rm~~      op ~Rn~~ ~Rd~~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 10) | (rn << 5) | rd);
}

static void create_bin_add_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::z_type_variant zv) {
	create_bin_addsub_zzz<0>(bv, rd, rn, rm, zv);
}

static void create_bin_sub_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::z_type_variant zv) {
	create_bin_addsub_zzz<1>(bv, rd, rn, rm, zv);
}

template <uint32_t opa, uint32_t opb>
static void create_bin_addsubmul_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                                     aarch64::z_type_variant zv) {
	sloejit_assert(rdn < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rm < 32);
	// add_z_p_zz.xml
	uint32_t base = 0b00000100'00'0'0'000'0'000'000'00000'00000u;
	//                         sz  opb   opa    Pg~ ~Rm~~ ~Rdn~
	//                       22-^      16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (opb << 20) | (opa << 16) | (pg << 10) | (rm << 5) | rdn);
}

static void create_bin_add_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                               aarch64::z_type_variant zv) {
	create_bin_addsubmul_zpz<0, 0>(bv, rdn, pg, rm, zv);
}

static void create_bin_sub_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                               aarch64::z_type_variant zv) {
	create_bin_addsubmul_zpz<1, 0>(bv, rdn, pg, rm, zv);
}

static void create_bin_mul_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                               aarch64::z_type_variant zv) {
	create_bin_addsubmul_zpz<0, 1>(bv, rdn, pg, rm, zv);
}

template <uint32_t op>
static void create_bin_faddsub_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fadd_z_zz.xml
	uint32_t base = 0b01100101'00'0'00000'00000'0'00000'00000u;
	//                         sz   ~Rm~~      op ~Rn~~ ~Rd~~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_hsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 10) | (rn << 5) | rd);
}

static void create_bin_fadd_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant zv) {
	create_bin_faddsub_zzz<0>(bv, rd, rn, rm, zv);
}

static void create_bin_fsub_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant zv) {
	create_bin_faddsub_zzz<1>(bv, rd, rn, rm, zv);
}

static void create_bin_fmul_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant zv) {
	create_bin_faddsub_zzz<2>(bv, rd, rn, rm, zv);
}

template <uint32_t op>
static void create_bin_faddsub_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                                   aarch64::z_type_variant zv) {
	sloejit_assert(rdn < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rm < 32);
	// fadd_z_p_zz.xml
	uint32_t base = 0b01100101'00'00000'0'100'000'00000'00000u;
	//                         sz      op     Pg~ ~Rm~~ ~Rdn~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_hsd(zv);
	bv.push_u32(base | (sz << 22) | (op << 16) | (pg << 10) | (rm << 5) | rdn);
}

static void create_bin_fadd_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                                aarch64::z_type_variant zv) {
	create_bin_faddsub_zpz<0>(bv, rdn, pg, rm, zv);
}

static void create_bin_fsub_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                                aarch64::z_type_variant zv) {
	create_bin_faddsub_zpz<1>(bv, rdn, pg, rm, zv);
}

static void create_bin_fmul_zpz(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t rm,
                                aarch64::z_type_variant zv) {
	create_bin_faddsub_zpz<2>(bv, rdn, pg, rm, zv);
}

template <uint32_t ft>
static void create_bin_fneg_ff(bytevector &bv, uint32_t rd, uint32_t rn) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// fneg_float.xml
	uint32_t base = 0b00011110'00'100001010000'00000'00000u;
	//                         ft              ~Rn~~ ~Rd~~
	//                       22-^                5-^   0-^
	bv.push_u32(base | (ft << 22) | (rn << 5) | rd);
}

static void create_bin_fneg_hh(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fneg_ff<0b11>(bv, rd, rn);
}

static void create_bin_fneg_ss(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fneg_ff<0b00>(bv, rd, rn);
}

static void create_bin_fneg_dd(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fneg_ff<0b01>(bv, rd, rn);
}

static void create_bin_fneg_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// fneg_advsimd.xml
	uint32_t base = 0b0'0'1011101'0'1'00'000111110'00000'00000u;
	//                  Q        sz   ft           ~Rn~~ ~Rd~~
	//               30-^      22-^    ^-19          5-^   0-^
	uint32_t q = 0, sz = 0, ftype = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b1;
		ftype = 0b11;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b1;
		ftype = 0b11;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b0;
		ftype = 0b00;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b0;
		ftype = 0b00;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b1;
		ftype = 0b00;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (ftype << 19) | (rn << 5) | rd);
}

static void create_bin_fneg_zpz(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t rn,
                                aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	// fneg_z_p_z.xml
	uint32_t base = 0b00000100'00'011101101'000'00000'00000u;
	//                         sz           Pg~ ~Rn~~ ~Rd~~
	//                       22-^          10-^   5-^   0-^
	uint32_t sz = get_zv_sz_hsd(zv);
	bv.push_u32(base | (sz << 22) | (pg << 10) | (rn << 5) | rd);
}

template <uint32_t op>
static void create_bin_fmlas_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fmla_advsimd_vec.xml
	// fmls_advsimd_vec.xml
	uint32_t base = 0b0'0'001110'0'00'00000'00'0011'00000'00000u;
	//                  Q       op sz ~Rm~~ ft      ~Rn~~ ~Rd~~
	//               30-^        21-^  16-^  ^-14     5-^   0-^
	uint32_t q = 0, sz = 0, ftype = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b10;
		ftype = 0b00;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b10;
		ftype = 0b00;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b01;
		ftype = 0b11;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b01;
		ftype = 0b11;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		ftype = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (op << 23) | (sz << 21) | (rm << 16) | (ftype << 14) | (rn << 5) | rd);
}

static void create_bin_fmla_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_fmlas_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_fmls_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_fmlas_qqq<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_fmlas_zpzz(bytevector &bv, uint32_t rda, uint32_t pg, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant zv) {
	sloejit_assert(rda < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fmla_z_p_zzz.xml
	// fmls_z_p_zzz.xml
	uint32_t base = 0b01100101'00'1'00000'00'0'000'00000'00000u;
	//                         sz   ~Rm~~   op Pg~ ~Rn~~ ~Rda~
	//                       22-^    16-^     10-^   5-^   0-^
	uint32_t sz = get_zv_sz_hsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 13) | (pg << 10) | (rn << 5) | rda);
}

static void create_bin_fmla_zpzz(bytevector &bv, uint32_t rda, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	create_bin_fmlas_zpzz<0>(bv, rda, pg, rn, rm, zv);
}

static void create_bin_fmls_zpzz(bytevector &bv, uint32_t rda, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	create_bin_fmlas_zpzz<1>(bv, rda, pg, rn, rm, zv);
}

template <uint32_t op>
static void create_bin_fmlas_qqql(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                  aarch64::q_type_variant qv) {
	sloejit_assert(rda < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fmla_advsimd_elt.xml
	// fmls_advsimd_elt.xml
	uint32_t base = 0b0'0'001111'00'0'0'0000'00'01'0'0'00000'00000u;
	//                  Q        sz L M ~Rm~ op    H   ~Rn~~ ~Rd~~
	//               30-^      22-^     16-^    11-^     5-^   0-^
	uint32_t q = 0, sz = 0, h = 0, m = 0, l = 0;
	switch (qv) {
	case aarch64::qv_4h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 16);
		q = 0;
		sz = 0b00;
		h = lane >> 2;
		l = (lane & 0b10) >> 1;
		m = lane & 0b1;
		break;
	case aarch64::qv_8h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 16);
		q = 1;
		sz = 0b00;
		h = lane >> 2;
		l = (lane & 0b10) >> 1;
		m = lane & 0b1;
		break;
	case aarch64::qv_2s:
		sloejit_assert(lane < 4);
		q = 0;
		sz = 0b10;
		h = lane >> 1;
		l = lane & 0b1;
		m = rm >> 4;
		break;
	case aarch64::qv_4s:
		sloejit_assert(lane < 4);
		q = 1;
		sz = 0b10;
		h = lane >> 1;
		l = lane & 0b1;
		m = rm >> 4;
		break;
	case aarch64::qv_2d:
		sloejit_assert(lane < 2);
		q = 1;
		sz = 0b11;
		h = lane;
		l = 0;
		m = rm >> 4;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (l << 21) | (m << 20) | ((rm & 0xf) << 16) | (op << 14) |
	            (h << 11) | (rn << 5) | rda);
}

static void create_bin_fmla_qqql(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                 aarch64::q_type_variant qv) {
	create_bin_fmlas_qqql<0>(bv, rda, rn, rm, lane, qv);
}

static void create_bin_fmls_qqql(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                 aarch64::q_type_variant qv) {
	create_bin_fmlas_qqql<1>(bv, rda, rn, rm, lane, qv);
}

static void create_bin_fmul_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, unsigned lane,
                                 aarch64::q_type_variant qv) {
	create_bin_fmlas_qqql<2>(bv, rd, rn, rm, lane, qv);
}

template <uint32_t op>
static void create_bin_fmlas_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                  aarch64::z_type_variant zv) {
	sloejit_assert(rda < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// fmla_z_zzzi.xml
	// fmls_z_zzzi.xml
	uint32_t base = 0b01100100'0'0'1'00'000'00000'0'00000'00000u;
	//                        sz h   l~ Zm~      op ~Zn~~ ~Zda~
	//                        22-^ 19-^   ^-16 10-^   5-^   0-^
	uint32_t sz = 0, h = 0, l = 0;
	switch (zv) {
	case aarch64::zv_h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 8);
		sz = 0b0;
		h = lane >> 2;
		l = lane & 0b11;
		break;
	case aarch64::zv_s:
		sloejit_assert(lane < 4);
		sloejit_assert(rm < 8);
		sz = 0b1;
		h = 0b0;
		l = lane;
		break;
	case aarch64::zv_d:
		sloejit_assert(lane < 2);
		sloejit_assert(rm < 16);
		sz = 0b1;
		h = 0b1;
		l = lane << 1;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 23) | (h << 22) | (l << 19) | (rm << 16) | (op << 10) | (rn << 5) | rda);
}

static void create_bin_fmla_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                 aarch64::z_type_variant zv) {
	create_bin_fmlas_zzzl<0>(bv, rda, rn, rm, lane, zv);
}

static void create_bin_fmls_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                 aarch64::z_type_variant zv) {
	create_bin_fmlas_zzzl<1>(bv, rda, rn, rm, lane, zv);
}

static void create_bin_fcmla_qqqi(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t rot,
                                  aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(rot == 0 || rot == 90 || rot == 180 || rot == 270);
	rot /= 90;
	// fcmla_advsimd_vec.xml
	uint32_t base = 0b0'0'101110'00'0'00000'110'00'1'00000'00000u;
	//                  Q        sz   ~Rm~~    rot   ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^   11-^     5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (rot << 11) | (rn << 5) | rd);
}

static void create_bin_fcmla_zpzzi(bytevector &bv, uint32_t rda, uint32_t pg, uint32_t rn, uint32_t rm,
                                   uint32_t rot, aarch64::z_type_variant zv) {
	sloejit_assert(rda < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(rot == 0 || rot == 90 || rot == 180 || rot == 270);
	rot /= 90;
	// fcmla_z_p_zzz.xml
	uint32_t base = 0b01100100'00'0'00000'0'00'000'00000'00000u;
	//                         sz   ~Rm~~  rot Pg~ ~Rn~~ ~Rda~
	//                       22-^    16-^     10-^   5-^   0-^
	uint32_t sz = get_zv_sz_hsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (rot << 13) | (pg << 10) | (rn << 5) | rda);
}

template <uint32_t i, uint32_t sz>
static void create_bin_cntinc_r(bytevector &bv, uint32_t rd) {
	sloejit_assert(rd < 32);
	// incb_r_rs.xml
	// cntb_r_s.xml
	// note: we don't care about the pattern or multiplier for now, so ignoring it.
	uint32_t base = 0b00000100'00'1'0'0000'111000'00000'00000u;
	//                         sz   i imm~        ~pat~ ~Rd~~
	//                       22-^     16-^          5-^   0-^
	uint32_t pat = 0b11111;
	uint32_t imm = 0b0000;
	bv.push_u32(base | (sz << 22) | (i << 20) | (imm << 16) | (pat << 5) | rd);
}

static void create_bin_cntb_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b0, 0b00>(bv, rd);
}

static void create_bin_cnth_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b0, 0b01>(bv, rd);
}

static void create_bin_cntw_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b0, 0b10>(bv, rd);
}

static void create_bin_cntd_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b0, 0b11>(bv, rd);
}

static void create_bin_cntp_rpp(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t pn, aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(pg < 16);
	sloejit_assert(pn < 16);
	// cntp_r_p_p.xml
	uint32_t base = 0b00100101'00'10000010'0000'0'0000'00000;
	//                         sz          ~Pg~   ~Pn~ ~~Rd~
	//                       22-^          10-^    5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (pg << 10) | (pn << 5) | rd);
}

static void create_bin_incb_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b1, 0b00>(bv, rd);
}

static void create_bin_inch_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b1, 0b01>(bv, rd);
}

static void create_bin_incw_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b1, 0b10>(bv, rd);
}

static void create_bin_incd_r(bytevector &bv, uint32_t rd) {
	return create_bin_cntinc_r<0b1, 0b11>(bv, rd);
}

template<uint32_t one_if_sme>
void create_bin_addvl_rri_maybe_sme(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm) {
	static_assert(one_if_sme < 2);
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm << (32 - 6)) >> (32 - 6) == imm);
	// addvl_r_ri.xml
	// addsvl_r_ri.xml
	uint32_t base = 0b00000100001'00000'0101'0'000000'00000u;
	//                            ~Rn~~      ^ ~imm~~  ~Rd~~
	//                             16-^   11-|    5-^    0-^
	//                            one_if_sme-|
	bv.push_u32(base | (rn << 16) | (one_if_sme << 11) | ((static_cast<uint32_t>(imm) & 0x3fu) << 5) | rd);
}

static void create_bin_addvl_rri(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm) {
  return create_bin_addvl_rri_maybe_sme<0>(bv, rd, rn, imm);
}

static void create_bin_addsvl_rri(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm) {
  return create_bin_addvl_rri_maybe_sme<1>(bv, rd, rn, imm);
}

template <uint32_t op>
static void create_bin_index_zxx(bytevector &bv, uint32_t rd, uint32_t a, uint32_t b,
                                 aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(a < 32);
	sloejit_assert(b < 32);
	// index_z_ii.xml
	// index_z_ir.xml
	// index_z_ri.xml
	// index_z_rr.xml
	uint32_t base = 0b00000100'00'1'00000'0100'00'00000'00000u;
	//                         sz   ~~B~~      op ~~A~~ ~Rd~~
	//                       22-^    16-^    10-^  5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (b << 16) | (op << 10) | (a << 5) | rd);
}

static void create_bin_index_zii(bytevector &bv, uint32_t rd, int32_t imm, int32_t immb,
                                 aarch64::z_type_variant zv) {
	sloejit_assert((imm << (32 - 5)) >> (32 - 5) == imm);
	sloejit_assert((immb << (32 - 5)) >> (32 - 5) == immb);
	create_bin_index_zxx<0b00>(bv, rd, (static_cast<uint32_t>(imm) & 0x1fu),
	                           (static_cast<uint32_t>(immb) & 0x1fu), zv);
}

static void create_bin_index_zir(bytevector &bv, uint32_t rd, int32_t imm, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	sloejit_assert((imm << (32 - 5)) >> (32 - 5) == imm);
	create_bin_index_zxx<0b10>(bv, rd, (static_cast<uint32_t>(imm) & 0x1fu), rm, zv);
}

static void create_bin_index_zri(bytevector &bv, uint32_t rd, uint32_t rn, int32_t imm,
                                 aarch64::z_type_variant zv) {
	sloejit_assert((imm << (32 - 5)) >> (32 - 5) == imm);
	create_bin_index_zxx<0b01>(bv, rd, rn, (static_cast<uint32_t>(imm) & 0x1fu), zv);
}

static void create_bin_index_zrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	create_bin_index_zxx<0b11>(bv, rd, rn, rm, zv);
}

static void create_bin_and_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// and_log_shift.xml
	uint32_t base = 0b1'0001010'00'0'00000'000000'00000'00000u;
	//               sf         sh   ~Rm~~ ~imm~~ ~Rn~~ ~Rd~~
	//             31-^       22-^    16-^   10-^   5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_and_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// and_advsimd.xml
	uint32_t base = 0b0'0'001110001'00000'000111'00000'00000u;
	//                  Q           ~Rm~~        ~Rn~~ ~Rd~~
	//               30-^            16-^          5-^   0-^
	uint32_t q = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		break;
	case aarch64::qv_16b:
		q = 1;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_and_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// and_z_zz.xml
	uint32_t base = 0b00000100001'00000'001100'00000'00000u;
	//                            ~Rm~~        ~Rn~~ ~Rd~~
	//                             16-^          5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_orr_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// orr_log_shift.xml
	uint32_t base = 0b1'0101010'00'0'00000'000000'00000'00000u;
	//               sf         sh   ~Rm~~ ~imm~~ ~Rn~~ ~Rd~~
	//             31-^       22-^    16-^   10-^   5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_orr_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// orr_advsimd_reg.xml
	uint32_t base = 0b0'0'001110101'00000'000111'00000'00000u;
	//                  Q           ~Rm~~        ~Rn~~ ~Rd~~
	//               30-^            16-^          5-^   0-^
	uint32_t q = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		break;
	case aarch64::qv_16b:
		q = 1;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_orr_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// orr_z_zz.xml
	uint32_t base = 0b00000100011'00000'001100'00000'00000u;
	//                            ~Rm~~        ~Rn~~ ~Rd~~
	//                             16-^          5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_csel_rrri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t cond) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert((cond & 0xfu) == cond);
	// csel.xml
	// note: we only care about 64-bit variants for now, hence sf=1.
	uint32_t base = 0b1'0011010100'00000'0000'00'00000'00000u;
	//               sf            ~Rm~~ cond    ~Rn~~ ~Rd~~
	//             31-^             16-^    ^-12   5-^   0-^
	bv.push_u32(base | (rm << 16) | (cond << 12) | (rn << 5) | rd);
}

template <uint32_t u, uint32_t op>
static void create_bin_rev_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// rev32_advsimd.xml
	// rev64_advsimd.xml
	uint32_t base = 0b0'0'0'01110'00'100000000'0'10'00000'00000u;
	//                  Q U       sz          op    ~Rn~~ ~Rd~~
	//               30-^ ^-29  22-^        12-^      5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		sloejit_assert(!op);
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		sloejit_assert(!op);
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		sloejit_assert(!u && !op);
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		sloejit_assert(!u && !op);
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (u << 29) | (sz << 22) | (op << 12) | (rn << 5) | rd);
}

static void create_bin_rev16_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	return create_bin_rev_qq<0, 1>(bv, rd, rn, qv);
}

static void create_bin_rev32_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	return create_bin_rev_qq<1, 0>(bv, rd, rn, qv);
}

static void create_bin_rev64_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	return create_bin_rev_qq<0, 0>(bv, rd, rn, qv);
}

template <uint32_t op, uint32_t sz>
static void create_bin_revx_zpz(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t rn) {
	sloejit_assert(rd < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	// revb_z_z.xml
	uint32_t base = 0b00000101'00'1001'00'100'000'00000'00000u;
	//                         sz      op     Pg~ ~Rn~~ ~Rd~~
	//                       22-^    16-^    10-^   5-^   0-^
	bv.push_u32(base | (sz << 22) | (op << 16) | (pg << 10) | (rn << 5) | rd);
}

static void create_bin_revb_zpz(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t rn,
                                aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_h: return create_bin_revx_zpz<0b00, 0b01>(bv, rd, pg, rn);
	case aarch64::zv_s: return create_bin_revx_zpz<0b00, 0b10>(bv, rd, pg, rn);
	case aarch64::zv_d: return create_bin_revx_zpz<0b00, 0b11>(bv, rd, pg, rn);
	default: sloejit_assert(false);
	}
}

static void create_bin_revh_zpz(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t rn,
                                aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_s: return create_bin_revx_zpz<0b01, 0b10>(bv, rd, pg, rn);
	case aarch64::zv_d: return create_bin_revx_zpz<0b01, 0b11>(bv, rd, pg, rn);
	default: sloejit_assert(false);
	}
}

static void create_bin_revw_zpz(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t rn,
                                aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d: return create_bin_revx_zpz<0b10, 0b11>(bv, rd, pg, rn);
	default: sloejit_assert(false);
	}
}

static void create_bin_ext_zzi(bytevector &bv, uint32_t rdn, uint32_t rm, uint32_t imm) {
	sloejit_assert(rdn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(imm < 256);
	// ext_z_zi.xml
	uint32_t base = 0b00000101001'00000'000'000'00000'00000u;
	//                            imm8h   imi8l ~Rm~~ ~Zdn~
	//                             20-^    12-^   5-^   0-^
	uint32_t imm8h = imm >> 3;
	uint32_t imm8l = imm & 0x7u;
	bv.push_u32(base | (imm8h << 16) | (imm8l << 10) | (rm << 5) | rdn);
}

static void create_bin_ext_qqqi(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t idx,
                               aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(idx < 16);
	// ext_advsimd.xml
	uint32_t base = 0b0'0'101110000'00000'0'0000'0'00000'00000u;
	//                  Q           ~Rm~~   imm4   ~Rn~~ ~Rd~~
	//               30-^            16-^   11-^     5-^   0-^
	uint32_t q = 0;
	switch (qv) {
	case aarch64::qv_8b: q = 0; break;
	case aarch64::qv_16b: q = 1; break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (rm << 16) | (idx << 11) | (rn << 5) | rd);
}

static void create_bin_rev_zz(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// rev_z_z.xml
	uint32_t base = 0b00000101'00'111000001110'00000'00000;
	//                         sz              ~Zn~~ ~Zd~~
	//                       22-^                5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rn << 5) | rd);
}

static void create_bin_rev_pp(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::z_type_variant zv) {
	sloejit_assert(rd < 16);
	sloejit_assert(rn < 16);
	// rev_p_p.xml
	uint32_t base = 0b00000101'00'1101000100000'0000'0'0000;
	//                         sz               ~Pn~   ~Pd~
	//                       22-^                5-^    0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rn << 5) | rd);
}

template <uint32_t op>
static void create_bin_uzp_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// uzp1_advsimd.xml
	// uzp2_advsimd.xml
	uint32_t base = 0b0'0'001110'00'0'00000'0'0'0110'00000'00000u;
	//                  Q        sz   ~Rm~~   o      ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^   ^-14     5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (op << 14) | (rn << 5) | rd);
}

static void create_bin_uzp1_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_uzp_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_uzp2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_uzp_qqq<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_trn_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// trn1_advsimd.xml
	// trn2_advsimd.xml
	uint32_t base = 0b0'0'001110'00'0'00000'0'0'1010'00000'00000u;
	//                  Q        sz   ~Rm~~   o      ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^   ^-14     5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (op << 14) | (rn << 5) | rd);
}

static void create_bin_trn1_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_trn_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_trn2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_trn_qqq<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_zip_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// zip1_advsimd.xml
	// zip2_advsimd.xml
	uint32_t base = 0b0'0'001110'00'0'00000'0'0'1110'00000'00000u;
	//                  Q        sz   ~Rm~~   o      ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^   ^-14     5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (op << 14) | (rn << 5) | rd);
}

static void create_bin_zip1_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_zip_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_zip2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::q_type_variant qv) {
	create_bin_zip_qqq<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_uzp_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// uzp1_z_zz.xml
	uint32_t base = 0b00000101'00'1'00000'01101'0'00000'00000u;
	//                         sz   ~Rm~~       o ~Rn~~ ~Rd~~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 10) | (rn << 5) | rd);
}

static void create_bin_uzp1_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant qv) {
	create_bin_uzp_zzz<0>(bv, rd, rn, rm, qv);
}

static void create_bin_uzp2_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant qv) {
	create_bin_uzp_zzz<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_trn_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// trn1_z_zz.xml
	uint32_t base = 0b00000101'00'1'00000'01110'0'00000'00000u;
	//                         sz   ~Rm~~       o ~Rn~~ ~Rd~~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 10) | (rn << 5) | rd);
}

static void create_bin_trn1_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant qv) {
	create_bin_trn_zzz<0>(bv, rd, rn, rm, qv);
}

static void create_bin_trn2_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant qv) {
	create_bin_trn_zzz<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_zip_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                               aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// zip1_z_zz.xml
	uint32_t base = 0b00000101'00'1'00000'01100'0'00000'00000u;
	//                         sz   ~Rm~~       o ~Rn~~ ~Rd~~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 10) | (rn << 5) | rd);
}

static void create_bin_zip1_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant qv) {
	create_bin_zip_zzz<0>(bv, rd, rn, rm, qv);
}

static void create_bin_zip2_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                aarch64::z_type_variant qv) {
	create_bin_zip_zzz<1>(bv, rd, rn, rm, qv);
}

static void create_bin_splice_zpz(bytevector &bv, uint32_t rdn, uint32_t pv, uint32_t rm,
                                aarch64::z_type_variant zv) {
	sloejit_assert(rdn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(pv < 8);
	// splice_z_p_zz.xml
	uint32_t base = 0b00000101'00'101100100'000'00000'00000;
	//                         sz           ~Pv ~~Zm~ ~Zdn~
	//                       22-^          10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (pv << 10) | (rm << 5) | rdn);
}

template <uint32_t op>
static void create_bin_zip_trn_uzp_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                       aarch64::z_type_variant zv) {
	sloejit_assert(pd < 16);
	sloejit_assert(pn < 16);
	sloejit_assert(pm < 16);
	// trn1_p_pp.xml
	// uzp1_p_pp.xml
	// zip1_p_pp.xml
	uint32_t base = 0b00000101'00'10'0000'010'000'0'0000'0'0000;
	//                         sz    ~Pm~     op~   ~Pn~   ~Pd~
	//                       22-^    16-^    10-^    5-^    0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (pm << 16) | (op << 10) | (pn << 5) | pd);
}

static void create_bin_trn1_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                aarch64::z_type_variant zv) {
	return create_bin_zip_trn_uzp_ppp<0b100>(bv, pd, pn, pm, zv);
}

static void create_bin_trn2_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                aarch64::z_type_variant zv) {
	return create_bin_zip_trn_uzp_ppp<0b101>(bv, pd, pn, pm, zv);
}

static void create_bin_uzp1_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                aarch64::z_type_variant zv) {
	return create_bin_zip_trn_uzp_ppp<0b010>(bv, pd, pn, pm, zv);
}

static void create_bin_uzp2_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                aarch64::z_type_variant zv) {
	return create_bin_zip_trn_uzp_ppp<0b011>(bv, pd, pn, pm, zv);
}

static void create_bin_zip1_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                aarch64::z_type_variant zv) {
	return create_bin_zip_trn_uzp_ppp<0b000>(bv, pd, pn, pm, zv);
}

static void create_bin_zip2_ppp(bytevector &bv, uint32_t pd, uint32_t pn, uint32_t pm,
                                aarch64::z_type_variant zv) {
	return create_bin_zip_trn_uzp_ppp<0b001>(bv, pd, pn, pm, zv);
}

static void create_bin_dup_zi(bytevector &bv, uint32_t rd, int imm, aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert((imm << (32 - 8)) >> (32 - 8) == imm);
	// dup_z_i.xml
	// note: we don't care about the shift, so just ignore it for now.
	uint32_t base = 0b00100101'00'11100011'0'00000000'00000u;
	//                         sz         sh ~~imm8~~ ~Zd~~
	//                       22-^       13-^      5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | ((static_cast<uint32_t>(imm) & 0xffu) << 5) | rd);
}

static void create_bin_dup_zzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t lane,
                               aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// dup_z_zi.xml
	uint32_t base = 0b00000101'00'1'00000'001000'00000'00000u;
	//                         sz   ~tsz~        ~Zn~~ ~Zd~~
	//                       22-^    16-^          5-^   0-^
	uint32_t sz = 0, tsz = 0;
	switch (zv) {
	case aarch64::zv_b:
		sloejit_assert(lane < 64);
		sz = lane >> 4;
		tsz = 0b00001 | ((lane << 1) & 0b11110u);
		break;
	case aarch64::zv_h:
		sloejit_assert(lane < 32);
		sz = lane >> 3;
		tsz = 0b00010 | ((lane << 2) & 0b11100u);
		break;
	case aarch64::zv_s:
		sloejit_assert(lane < 16);
		sz = lane >> 2;
		tsz = 0b00100 | ((lane << 3) & 0b11000u);
		break;
	case aarch64::zv_d:
		sloejit_assert(lane < 8);
		sz = lane >> 1;
		tsz = 0b01000 | ((lane << 4) & 0b10000u);
		break;
	case aarch64::zv_q:
		sloejit_assert(lane < 4);
		sz = lane;
		tsz = 0b10000;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 22) | (tsz << 16) | (rn << 5) | rd);
}

static void create_bin_dup_qr(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// dup_advsimd_gen.xml
	uint32_t base = 0b0'0'001110000'00000'000011'00000'00000u;
	//                  Q           ~imm5        ~Rn~~ ~Rd~~
	//               30-^            16-^          5-^   0-^
	uint32_t q = 0, imm5 = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		imm5 = 0b00001u;
		break;
	case aarch64::qv_16b:
		q = 1;
		imm5 = 0b00001u;
		break;
	case aarch64::qv_4h:
		q = 0;
		imm5 = 0b00010u;
		break;
	case aarch64::qv_8h:
		q = 1;
		imm5 = 0b00010u;
		break;
	case aarch64::qv_2s:
		q = 0;
		imm5 = 0b00100u;
		break;
	case aarch64::qv_4s:
		q = 1;
		imm5 = 0b00100u;
		break;
	case aarch64::qv_2d:
		q = 1;
		imm5 = 0b01000u;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (imm5 << 16) | (rn << 5) | rd);
}

static void create_bin_dup_qql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t lane,
                               aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// dup_advsimd_elt.xml
	uint32_t base = 0b0'0'001110000'00000'000001'00000'00000u;
	//                  Q           ~imm5        ~Rn~~ ~Rd~~
	//               30-^            16-^          5-^   0-^
	uint32_t q = 0, imm5 = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sloejit_assert(lane < 16);
		imm5 = static_cast<uint32_t>((lane << 1) | 0b00001u);
		break;
	case aarch64::qv_16b:
		q = 1;
		sloejit_assert(lane < 16);
		imm5 = static_cast<uint32_t>((lane << 1) | 0b00001u);
		break;
	case aarch64::qv_4h:
		q = 0;
		sloejit_assert(lane < 8);
		imm5 = static_cast<uint32_t>((lane << 2) | 0b00010u);
		break;
	case aarch64::qv_8h:
		q = 1;
		sloejit_assert(lane < 8);
		imm5 = static_cast<uint32_t>((lane << 2) | 0b00010u);
		break;
	case aarch64::qv_2s:
		q = 0;
		sloejit_assert(lane < 4);
		imm5 = static_cast<uint32_t>((lane << 3) | 0b00100u);
		break;
	case aarch64::qv_4s:
		q = 1;
		sloejit_assert(lane < 4);
		imm5 = static_cast<uint32_t>((lane << 3) | 0b00100u);
		break;
	case aarch64::qv_2d:
		q = 1;
		sloejit_assert(lane < 2);
		imm5 = static_cast<uint32_t>((lane << 4) | 0b01000u);
		break;
	default:
		sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (imm5 << 16) | (rn << 5) | rd);
}

template <uint32_t l>
static void create_bin_fcvt_qq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t sz) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// fcvtl_advsimd.xml
	// fcvtn_advsimd.xml
	uint32_t base = 0b000011100'0'100001011'0'10'00000'00000u;
	//                         sz           l    ~Rn~~ ~Rd~~
	//                       22-^        12-^      5-^   0-^
	bv.push_u32(base | (sz << 22) | (l << 12) | (rn << 5) | rd);
}

static void create_bin_fcvtl_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant to,
                                aarch64::q_type_variant from) {
	uint32_t sz = 0;
	switch (to) {
	case aarch64::qv_4s:
		sloejit_assert(from == aarch64::qv_4h);
		sz = 0;
		break;
	case aarch64::qv_2d:
		sloejit_assert(from == aarch64::qv_2s);
		sz = 1;
		break;
	default: sloejit_assert(false);
	}
	create_bin_fcvt_qq<1>(bv, rd, rn, sz);
}

static void create_bin_fcvtn_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant to,
                                aarch64::q_type_variant from) {
	uint32_t sz = 0;
	switch (from) {
	case aarch64::qv_4s:
		sloejit_assert(to == aarch64::qv_4h);
		sz = 0;
		break;
	case aarch64::qv_2d:
		sloejit_assert(to == aarch64::qv_2s);
		sz = 1;
		break;
	default: sloejit_assert(false);
	}
	create_bin_fcvt_qq<0>(bv, rd, rn, sz);
}

template <uint32_t ft, uint32_t op>
static void create_bin_fcvt_ff(bytevector &bv, uint32_t rd, uint32_t rn) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	uint32_t base = 0b00011110'00'10001'00'10000'00000'00000u;
	//                         ft       op       ~Rn~~ ~Rd~~
	//                       22-^     15-^         5-^   0-^
	bv.push_u32(base | (ft << 22) | (op << 15) | (rn << 5) | rd);
}

static void create_bin_fcvt_sh(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fcvt_ff<0b11, 0b00>(bv, rd, rn);
}

static void create_bin_fcvt_dh(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fcvt_ff<0b11, 0b01>(bv, rd, rn);
}

static void create_bin_fcvt_hs(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fcvt_ff<0b00, 0b11>(bv, rd, rn);
}

static void create_bin_fcvt_ds(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fcvt_ff<0b00, 0b01>(bv, rd, rn);
}

static void create_bin_fcvt_hd(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fcvt_ff<0b01, 0b11>(bv, rd, rn);
}

static void create_bin_fcvt_sd(bytevector &bv, uint32_t rd, uint32_t rn) {
	create_bin_fcvt_ff<0b01, 0b00>(bv, rd, rn);
}

template <uint32_t sf, uint32_t ft, uint32_t rm, uint32_t op>
static void create_bin_fmov_xx(bytevector &bv, uint32_t rd, uint32_t rn) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// fmov_float_gen.xml
	uint32_t base = 0b0'0011110'00'1'00'000'000000'00000'00000u;
	//               sf         ft   rm op~        ~Rn~~ ~Rd~~
	//             31-^       22-^ 19-^   ^-16       5-^   0-^
	bv.push_u32(base | (sf << 31) | (ft << 22) | (rm << 19) | (op << 16) | (rn << 5) | rd);
}

static void create_bin_fmov_rh(bytevector &bv, uint32_t rd, uint32_t rn) {
	return create_bin_fmov_xx<0b0, 0b11, 0b00, 0b110>(bv, rd, rn);
}

static void create_bin_fmov_hr(bytevector &bv, uint32_t rd, uint32_t rn) {
	return create_bin_fmov_xx<0b0, 0b11, 0b00, 0b111>(bv, rd, rn);
}

static void create_bin_fmov_rs(bytevector &bv, uint32_t rd, uint32_t rn) {
	return create_bin_fmov_xx<0b0, 0b00, 0b00, 0b110>(bv, rd, rn);
}

static void create_bin_fmov_sr(bytevector &bv, uint32_t rd, uint32_t rn) {
	return create_bin_fmov_xx<0b0, 0b00, 0b00, 0b111>(bv, rd, rn);
}

static void create_bin_fmov_rd(bytevector &bv, uint32_t rd, uint32_t rn) {
	return create_bin_fmov_xx<0b1, 0b01, 0b00, 0b110>(bv, rd, rn);
}

static void create_bin_fmov_dr(bytevector &bv, uint32_t rd, uint32_t rn) {
	return create_bin_fmov_xx<0b1, 0b01, 0b00, 0b111>(bv, rd, rn);
}

static void create_bin_smov_rql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t lane,
                                aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	uint32_t imm5 = 0;
	switch (qv) {
	case aarch64::qv_8b:
		sloejit_assert(lane < 8);
		imm5 = (lane << 1) | 0b00001u;
		break;
	case aarch64::qv_16b:
		sloejit_assert(lane < 16);
		imm5 = (lane << 1) | 0b00001u;
		break;
	case aarch64::qv_4h:
		sloejit_assert(lane < 4);
		imm5 = (lane << 2) | 0b00010u;
		break;
	case aarch64::qv_8h:
		sloejit_assert(lane < 8);
		imm5 = (lane << 2) | 0b00010u;
		break;
	case aarch64::qv_4s:
		sloejit_assert(lane < 4);
		imm5 = (lane << 3) | 0b00100u;
		break;
	default: sloejit_assert(false);
	}
	// smov_advsimd.xml
	uint32_t base = 0b01001110000'00000'001011'00000'00000;
	//                            ~imm5        ~~Rn~ ~~Rd~
	//                             16-^          5-^   0-^
	bv.push_u32(base | (imm5 << 16) | (rn << 5) | rd);
}

static void create_bin_pfalse_p(bytevector &bv, uint32_t rd) {
	sloejit_assert(rd < 16);
	// pfalse_p.xml
	uint32_t base = 0b0010010100011000111001000000'0000;
	//                                             ~Rd~
	//                                              0-^
	bv.push_u32(base | rd);
}

static void create_bin_ptrue_p(bytevector &bv, uint32_t rd, aarch64::ptrue_pat pat,
                               aarch64::z_type_variant zv) {
	sloejit_assert(rd < 16);
	// ptrue_p_s.xml
	// note: we only care about the non-flag-setting variant for now, hence
	//       bit 16 is left as always zero.
	uint32_t base = 0b00100101'00'01100'0'111000'00000'0'0000;
	//                         sz       S        ~pat~   ~Rd~
	//                       22-^    16-^          5-^    0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	uint32_t pat_bits = static_cast<uint32_t>(pat);
	sloejit_assert(pat_bits < 32);
	bv.push_u32(base | (sz << 22) | (pat_bits << 5) | rd);
}

template <uint32_t op>
static void create_bin_whilexx_prr(bytevector &bv, uint32_t pd, uint32_t rn, uint32_t rm,
                                   aarch64::z_type_variant zv) {
	sloejit_assert(pd < 16);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// whilele_p_p_rr.xml
	// whilelt_p_p_rr.xml
	// note: we don't care about the 32-bit versions, so ignoring those.
	uint32_t base = 0b00100101'00'1'00000'000101'00000'0'0000u;
	//                         sz   ~Rm~~        ~Rn~~ o ~Pd~
	//                       22-^    16-^          5-^    0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (rn << 5) | (op << 4) | pd);
}

static void create_bin_whilelt_prr(bytevector &bv, uint32_t pd, uint32_t rn, uint32_t rm,
                                   aarch64::z_type_variant zv) {
	create_bin_whilexx_prr<0>(bv, pd, rn, rm, zv);
}

static void create_bin_whilele_prr(bytevector &bv, uint32_t pd, uint32_t rn, uint32_t rm,
                                   aarch64::z_type_variant zv) {
	create_bin_whilexx_prr<1>(bv, pd, rn, rm, zv);
}

template <uint32_t op>
static void create_bin_adrx_ri(bytevector &bv, uint32_t rd, int32_t imm) {
	sloejit_assert(rd < 32);
	sloejit_assert((imm << (32 - 21)) >> (32 - 21) == imm);
	// adr.xml
	// adrp.xml
	uint32_t base = 0b0'00'10000'0000000000000000000'00000u;
	//               op lo       ~~~~~~~immhi~~~~~~~ ~Rd~~
	//                29-^                       5-^   0-^
	uint32_t immlo = static_cast<uint32_t>(imm) & 0x3u;
	uint32_t immhi = (static_cast<uint32_t>(imm) >> 2u) & 0x7ffff;
	bv.push_u32(base | (op << 31) | (immlo << 29) | (immhi << 5) | rd);
}

static void create_bin_adr_ri(bytevector &bv, uint32_t rd, int64_t imm) {
	return create_bin_adrx_ri<0>(bv, rd, imm);
}

static void create_bin_adrp_ri(bytevector &bv, uint32_t rd, int64_t imm) {
	sloejit_assert((imm & 0xfffll) == 0ll);
	return create_bin_adrx_ri<1>(bv, rd, imm >> 12);
}

static void create_bin_ldrsb_x_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// ldrsb_reg.xml
	uint32_t base = 0b00111000101'00000'011'0'10'00000'00000u;
	//                            ~Rm~~          ~Rn~~ ~Rd~~
	//                             16-^            5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_ldrsb_x_rri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & 0xfffu) == imm);
	// ldrsb_imm.xml
	uint32_t base = 0b0011100110'000000000000'00000'00000u;
	//                           ~~~~Rm~~~~~~ ~Rn~~ ~Rd~~
	//                                   10-^   5-^   0-^
	bv.push_u32(base | (imm << 10) | (rn << 5) | rd);
}

static void create_bin_ldr_x_rrr(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm) {
	// TODO: this does not expose the LSL #3 variant, which would be useful
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// ldr_reg_gen.xml
	uint32_t base = 0b11111000011'00000'011'0'10'00000'00000u;
	//                            ~Rm~~          ~Rn~~ ~Rd~~
	//                             16-^            5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_ldr_x_rri(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t imm) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & (0xfffu << 3u)) == imm);
	imm >>= 3;
	// ldr_imm_gen.xml
	uint32_t base = 0b1111100101'000000000000'00000'00000u;
	//                           ~~~~Rm~~~~~~ ~Rn~~ ~Rd~~
	//                                   10-^   5-^   0-^
	bv.push_u32(base | (imm << 10) | (rn << 5) | rd);
}

template <uint32_t opc, uint32_t l>
static void create_bin_x_ldp_stp_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	sloejit_assert(rt1 < 32);
	sloejit_assert(rt2 < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & 0x7) == 0);
	imm >>= 3;
	sloejit_assert((imm << (32 - 7)) >> (32 - 7) == imm);
	// ldp_gen.xml
	// stp_gen.xml
	uint32_t base = 0b101010'000'0'0000000'00000'00000'00000u;
	//                       opc L ~imm7~~ ~Rt2~ ~Rn~~ ~Rt1~
	//                      23-^      15-^  10-^   5-^   0-^
	bv.push_u32(base | (opc << 23) | (l << 22) | ((static_cast<uint32_t>(imm) & 0x7fu) << 15) | (rt2 << 10) |
	            (rn << 5) | rt1);
}

static void create_bin_ldp_x_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	create_bin_x_ldp_stp_rrri<0b010, 0b1>(bv, rt1, rt2, rn, imm);
}

static void create_bin_ldp_x_preindex_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn,
                                           int32_t imm) {
	create_bin_x_ldp_stp_rrri<0b011, 0b1>(bv, rt1, rt2, rn, imm);
}

static void create_bin_ldp_x_postindex_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn,
                                            int32_t imm) {
	create_bin_x_ldp_stp_rrri<0b001, 0b1>(bv, rt1, rt2, rn, imm);
}

template <uint32_t opc, uint32_t l, int shift>
static void create_bin_f_ldp_stp_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	sloejit_assert(rt1 < 32);
	sloejit_assert(rt2 < 32);
	sloejit_assert(rn < 32);
	int32_t mask = (1 << shift) - 1;
	sloejit_assert((imm & mask) == 0);
	imm >>= shift;
	sloejit_assert((imm << (32 - 7)) >> (32 - 7) == imm);
	// ldp_fpsimd.xml
	// stp_fpsimd.xml
	// note: we only care about the signed offset version of both for now.
	uint32_t base = 0b00'1011010'0'0000000'00000'00000'00000u;
	//                op         L ~imm7~~ ~Rt2~ ~Rn~~ ~Rt1~
	//              30-^      22-^    15-^  10-^   5-^   0-^
	bv.push_u32(base | (opc << 30) | (l << 22) | ((static_cast<uint32_t>(imm) & 0x7fu) << 15) | (rt2 << 10) |
	            (rn << 5) | rt1);
}

static void create_bin_ldp_s_ssri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	create_bin_f_ldp_stp_rrri<0b00, 0b1, 2>(bv, rt1, rt2, rn, imm);
}

static void create_bin_ldp_d_ddri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	create_bin_f_ldp_stp_rrri<0b01, 0b1, 3>(bv, rt1, rt2, rn, imm);
}

static void create_bin_ldp_q_qqri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	create_bin_f_ldp_stp_rrri<0b10, 0b1, 4>(bv, rt1, rt2, rn, imm);
}

template <uint32_t op>
static void create_bin_x_movx_ri(bytevector &bv, uint32_t rd, uint64_t imm) {
	sloejit_assert(rd < 32);
	uint32_t shift = (imm & 0x0000'0000'0000'fffful) == imm
	                     ? 0
	                     : (imm & 0x0000'0000'ffff'0000ul) == imm
	                           ? 1
	                           : (imm & 0x0000'ffff'0000'0000ul) == imm
	                                 ? 2
	                                 : (imm & 0xffff'0000'0000'0000ul) == imm ? 3 : ~0u;
	sloejit_assert(shift != ~0u);
	imm = (imm >> (shift * 16ul)) & 0xffffu;
	// movk.xml
	// movn.xml
	// movz.xml
	uint32_t base = 0b1'00'100101'00'0000000000000000'00000u;
	//                  op        sh ~~~~~~imm~~~~~~~ ~Rd~~
	//                29-^      21-^              5-^   0-^
	bv.push_u32(base | (op << 29) | (shift << 21) | (imm << 5) | rd);
}

static void create_bin_mova_zpalorl(bytevector &bv, uint32_t zd, uint32_t pg, uint32_t za, uint32_t tile, uint32_t hv, uint32_t rs, uint32_t off, aarch64::z_type_variant zv) {
	const uint32_t size = get_zv_sz_bhsdq(zv);
	const bool is_q = zv == aarch64::z_type_variant::zv_q;
	const uint32_t q = is_q ? 1 : 0;
	const uint32_t msz = is_q ? 0b100 : size;
	sloejit_assert(zd < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(za == 0);
	sloejit_assert(tile < (1u << msz));
	sloejit_assert(hv < 2);
	sloejit_assert(rs >= 12 && rs <= 15);
	sloejit_assert(off < (1u << (4 - msz)));

	// mov_mova_z_p_rza.xml
	const uint32_t base = 0b11000000'00'00001'0'0'00'000'0'0000'00000;
	//                               sz       Q V Rs ~Pg  off/t ~~Zd~
	//                             22-^    16-^ 13-^10-^    5-^   0-^
	// The four bits labeled off/t are for the concatenation of tile index and offset, whose
	// individual widths vary according to msz but which always occupy 4 bits in total
	bv.push_u32(base | (size << 22) | (q << 16) | (hv << 15) | ((rs - 12) << 13) | (pg << 10) | (tile << (9 - msz)) | (off << 5) | zd);
}

static void create_bin_mova_alorlpz(bytevector &bv, uint32_t za, uint32_t tile, uint32_t hv, uint32_t rs, uint32_t off, uint32_t pg, uint32_t zn, aarch64::z_type_variant zv) {
	const uint32_t size = get_zv_sz_bhsdq(zv);
	const bool is_q = zv == aarch64::z_type_variant::zv_q;
	const uint32_t q = is_q ? 1 : 0;
	const uint32_t msz = is_q ? 0b100 : size;
	sloejit_assert(zn < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(za == 0);
	sloejit_assert(tile < (1u << msz));
	sloejit_assert(hv < 2);
	sloejit_assert(rs >= 12 && rs <= 15);
	sloejit_assert(off < (1u << (4 - msz)));

	// mov_mova_za_p_rz.xml
	const uint32_t base = 0b11000000'00'00000'0'0'00'000'00000'0'0000;
	//                               sz       Q V Rs ~Pg ~~Zn~  off/t
	//                             22-^    16-^ 13-^10-^   5-^    0-^
	// The four bits labeled off/t are for the concatenation of tile index and offset, whose
	// individual widths vary according to msz but which always occupy 4 bits in total
	bv.push_u32(base | (size << 22) | (q << 16) | (hv << 15) | ((rs - 12) << 13) | (pg << 10) | (zn << 5) | (tile << (4 - msz)) | off);
}

static void create_bin_movz_ri(bytevector &bv, uint32_t rd, uint64_t imm) {
	create_bin_x_movx_ri<0b10>(bv, rd, imm);
}

static void create_bin_movn_ri(bytevector &bv, uint32_t rd, uint64_t imm) {
	create_bin_x_movx_ri<0b00>(bv, rd, imm);
}

static void create_bin_movk_ri(bytevector &bv, uint32_t rd, uint64_t imm) {
	create_bin_x_movx_ri<0b11>(bv, rd, imm);
}

static void create_bin_strb_x_rrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// strb_reg.xml
	uint32_t base = 0b00111000001'00000'011010'00000'00000u;
	//                            ~Rm~~        ~Rn~~ ~Rt~~
	//                             16-^          5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rt);
}

static void create_bin_strb_x_rri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & 0xfffu) == imm);
	// strb_imm.xml
	uint32_t base = 0b0011100100'000000000000'00000'00000u;
	//                           ~~~~Rm~~~~~~ ~Rn~~ ~Rd~~
	//                                   10-^   5-^   0-^
	bv.push_u32(base | (imm << 10) | (rn << 5) | rt);
}

static void create_bin_str_x_rrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	// TODO: this does not expose the LSL #3 variant, which would be useful
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// str_reg_gen.xml
	uint32_t base = 0b11111000001'00000'011010'00000'00000u;
	//                            ~Rm~~        ~Rn~~ ~Rt~~
	//                             16-^          5-^   0-^
	bv.push_u32(base | (rm << 16) | (rn << 5) | rt);
}

static void create_bin_str_x_rri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & (0xfffu << 3u)) == imm);
	imm >>= 3;
	// str_imm_gen.xml
	uint32_t base = 0b1111100100'000000000000'00000'00000u;
	//                           ~~~~Rm~~~~~~ ~Rn~~ ~Rd~~
	//                                   10-^   5-^   0-^
	bv.push_u32(base | (imm << 10) | (rn << 5) | rt);
}

static void create_bin_stp_x_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn, int32_t imm) {
	create_bin_x_ldp_stp_rrri<0b010, 0b0>(bv, rt1, rt2, rn, imm);
}

static void create_bin_stp_x_preindex_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn,
                                           int32_t imm) {
	create_bin_x_ldp_stp_rrri<0b011, 0b0>(bv, rt1, rt2, rn, imm);
}

static void create_bin_stp_x_postindex_rrri(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn,
                                            int32_t imm) {
	create_bin_x_ldp_stp_rrri<0b001, 0b0>(bv, rt1, rt2, rn, imm);
}

template <uint32_t sz, uint32_t opc, uint32_t s>
static void create_bin_f_ldstr_rrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// ldr_reg_fpsimd.xml
	uint32_t base = 0b00'111100'00'1'00000'011'0'10'00000'00000u;
	//                sz        op   ~Rm~~     S    ~Rn~~ ~Rt~~
	//              30-^      22-^    16-^  12-^      5-^   0-^
	bv.push_u32(base | (sz << 30) | (opc << 22) | (rm << 16) | (s << 12) | (rn << 5) | rt);
}

template <uint32_t sz, uint32_t opc, uint32_t shift>
static void create_bin_f_ldstr_rri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm & (0xfffu << shift)) == imm);
	imm >>= shift;
	// ldr_imm_fpsimd.xml
	// str_imm_fpsimd.xml
	uint32_t base = 0b00'111101'00'000000000000'00000'00000u;
	//                sz        op ~~~imm12~~~~ ~Rn~~ ~Rt~~
	//              30-^      22-^         10-^   5-^   0-^
	bv.push_u32(base | (sz << 30) | (opc << 22) | (imm << 10) | (rn << 5) | rt);
}

static void create_bin_ldr_b_bri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b00, 0b01, 0>(bv, rt, rn, imm);
}

static void create_bin_ldr_b_brr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b00, 0b01, 0b0>(bv, rt, rn, rm);
}

static void create_bin_ldr_h_hri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b01, 0b01, 1>(bv, rt, rn, imm);
}

static void create_bin_ldr_h_hrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b01, 0b01, 0b1>(bv, rt, rn, rm);
}

static void create_bin_ldr_s_sri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b10, 0b01, 2>(bv, rt, rn, imm);
}

static void create_bin_ldr_s_srr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b10, 0b01, 0b1>(bv, rt, rn, rm);
}

static void create_bin_ldr_d_dri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b11, 0b01, 3>(bv, rt, rn, imm);
}

static void create_bin_ldr_d_drr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b11, 0b01, 0b1>(bv, rt, rn, rm);
}

static void create_bin_ldr_q_qri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b00, 0b11, 4>(bv, rt, rn, imm);
}

static void create_bin_ldr_q_qrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b00, 0b11, 0b1>(bv, rt, rn, rm);
}

static void create_bin_str_b_bri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b00, 0b00, 0>(bv, rt, rn, imm);
}

static void create_bin_str_b_brr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b00, 0b00, 0b0>(bv, rt, rn, rm);
}

static void create_bin_str_h_hri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b01, 0b00, 1>(bv, rt, rn, imm);
}

static void create_bin_str_h_hrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b01, 0b00, 0b1>(bv, rt, rn, rm);
}

static void create_bin_str_s_sri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b10, 0b00, 2>(bv, rt, rn, imm);
}

static void create_bin_str_s_srr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b10, 0b00, 0b1>(bv, rt, rn, rm);
}

static void create_bin_str_d_dri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b11, 0b00, 3>(bv, rt, rn, imm);
}

static void create_bin_str_d_drr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b11, 0b00, 0b1>(bv, rt, rn, rm);
}

static void create_bin_str_q_qri(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t imm) {
	create_bin_f_ldstr_rri<0b00, 0b10, 4>(bv, rt, rn, imm);
}

static void create_bin_str_q_qrr(bytevector &bv, uint32_t rt, uint32_t rn, uint32_t rm) {
	create_bin_f_ldstr_rrr<0b00, 0b10, 0b1>(bv, rt, rn, rm);
}

template <uint32_t op>
static void create_bin_ldstr_zri(bytevector &bv, uint32_t rt, uint32_t rn, int32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	sloejit_assert((imm << (32 - 9)) >> (32 - 9) == imm);
	// ldr_z_bi.xml
	// str_z_bi.xml
	uint32_t base = 0b1'00'0010110'000000'010'000'00000'00000u;
	//                  op         ~immb~     imm ~Rn~~ ~Rt~~
	//                29-^           16-^    10-^   5-^   0-^
	uint32_t imma = (static_cast<uint32_t>(imm) & 0b000000111u);
	uint32_t immb = (static_cast<uint32_t>(imm) & 0b111111000u) >> 3;
	bv.push_u32(base | (op << 29) | (immb << 16) | (imma << 10) | (rn << 5) | rt);
}

static void create_bin_ldr_zri(bytevector &bv, uint32_t rt, uint32_t rn, int32_t imm) {
	create_bin_ldstr_zri<0b00>(bv, rt, rn, imm);
}

static void create_bin_str_zri(bytevector &bv, uint32_t rt, uint32_t rn, int32_t imm) {
	create_bin_ldstr_zri<0b11>(bv, rt, rn, imm);
}

static void create_bin_ld1r_qr(bytevector &bv, uint32_t rt, uint32_t rn, aarch64::q_type_variant qv) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	// ld1r_advsimd.xml
	uint32_t base = 0b0'0'001101010000001100'00'00000'00000u;
	//                  Q                    sz ~Rn~~ ~Rt~~
	//               30-^                  10-^   5-^   0-^
	uint32_t sz = 0, q = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00u;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00u;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01u;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01u;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10u;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10u;
		break;
	case aarch64::qv_1d:
		q = 0;
		sz = 0b11u;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11u;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 10) | (rn << 5) | rt);
}

static void get_advsimd_ldnst_q_size(aarch64::q_type_variant qv, uint32_t &q, uint32_t &sz) {
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00u;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00u;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01u;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01u;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10u;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10u;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11u;
		break;
	default: sloejit_assert(false);
	}
}

static void assert_advsimd_ldnst_consecutive(uint32_t first, uint32_t next, uint32_t offset) {
	sloejit_assert(first < 32);
	sloejit_assert(next < 32);
	sloejit_assert(next == ((first + offset) & 31));
}

static void create_bin_ldnst_qr(bytevector &bv, uint32_t rt, uint32_t rn,
                                aarch64::q_type_variant qv, uint32_t opcode) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	// ld2_advsimd_mult.xml
	// ld3_advsimd_mult.xml
	// ld4_advsimd_mult.xml
	// st2_advsimd_mult.xml
	// st3_advsimd_mult.xml
	// st4_advsimd_mult.xml
	uint32_t base = 0b0'0'00'110'00'1'000000'0000'00'00000'00000u;
	//                  Q           L      opcode sz ~Rn~~ ~Rt~~
	//               30-^        22-^        12-^  ^   5-^   0-^
	//                                             |-10
	uint32_t q = 0, sz = 0;
	get_advsimd_ldnst_q_size(qv, q, sz);
	bv.push_u32(base | (q << 30) | (opcode << 12) | (sz << 10) | (rn << 5) | rt);
}

static void create_bin_stnst_qr(bytevector &bv, uint32_t rt, uint32_t rn,
                                aarch64::q_type_variant qv, uint32_t opcode) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	// st2_advsimd_mult.xml
	// st3_advsimd_mult.xml
	// st4_advsimd_mult.xml
	uint32_t base = 0b0'0'00'110'00'0'000000'0000'00'00000'00000u;
	//                  Q           L      opcode sz ~Rn~~ ~Rt~~
	//               30-^        22-^        12-^  ^   5-^   0-^
	//                                             |-10
	uint32_t q = 0, sz = 0;
	get_advsimd_ldnst_q_size(qv, q, sz);
	bv.push_u32(base | (q << 30) | (opcode << 12) | (sz << 10) | (rn << 5) | rt);
}

static void create_bin_ld2_qqr(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn,
                               aarch64::q_type_variant qv) {
	assert_advsimd_ldnst_consecutive(rt1, rt2, 1);
	create_bin_ldnst_qr(bv, rt1, rn, qv, 0b1000u);
}

static void create_bin_ld3_qqqr(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rt3, uint32_t rn,
                                aarch64::q_type_variant qv) {
	assert_advsimd_ldnst_consecutive(rt1, rt2, 1);
	assert_advsimd_ldnst_consecutive(rt1, rt3, 2);
	create_bin_ldnst_qr(bv, rt1, rn, qv, 0b0100u);
}

static void create_bin_ld4_qqqqr(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rt3, uint32_t rt4, uint32_t rn,
                                 aarch64::q_type_variant qv) {
	assert_advsimd_ldnst_consecutive(rt1, rt2, 1);
	assert_advsimd_ldnst_consecutive(rt1, rt3, 2);
	assert_advsimd_ldnst_consecutive(rt1, rt4, 3);
	create_bin_ldnst_qr(bv, rt1, rn, qv, 0b0000u);
}

static void create_bin_st2_qqr(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rn,
                               aarch64::q_type_variant qv) {
	assert_advsimd_ldnst_consecutive(rt1, rt2, 1);
	create_bin_stnst_qr(bv, rt1, rn, qv, 0b1000u);
}

static void create_bin_st3_qqqr(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rt3, uint32_t rn,
                                aarch64::q_type_variant qv) {
	assert_advsimd_ldnst_consecutive(rt1, rt2, 1);
	assert_advsimd_ldnst_consecutive(rt1, rt3, 2);
	create_bin_stnst_qr(bv, rt1, rn, qv, 0b0100u);
}

static void create_bin_st4_qqqqr(bytevector &bv, uint32_t rt1, uint32_t rt2, uint32_t rt3, uint32_t rt4, uint32_t rn,
                                 aarch64::q_type_variant qv) {
	assert_advsimd_ldnst_consecutive(rt1, rt2, 1);
	assert_advsimd_ldnst_consecutive(rt1, rt3, 2);
	assert_advsimd_ldnst_consecutive(rt1, rt4, 3);
	create_bin_stnst_qr(bv, rt1, rn, qv, 0b0000u);
}

static void create_bin_st1_q_qir(bytevector &bv, uint32_t rt, uint32_t idx, uint32_t rn,
                                 aarch64::q_type_variant qv) {
	sloejit_assert(rt < 32);
	sloejit_assert(rn < 32);
	// st1_advsimd_sngl.xml
	uint32_t base = 0b0'0'00110100000000'000'0'00'00000'00000u;
	//                  Q                opc S sz ~Rn~~ ~Rt~~
	//               30-^               13-^ 10-^   5-^   0-^
	uint32_t opc = 0, q = 0, s = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_16b:
		sloejit_assert(idx < 16);
		opc = 0b000;
		q = (idx & 0x8) >> 3;
		s = (idx & 0x4) >> 2;
		sz = idx & 0x3;
		break;
	case aarch64::qv_8h:
		sloejit_assert(idx < 8);
		opc = 0b010;
		q = (idx & 0x4) >> 2;
		s = (idx & 0x2) >> 1;
		sz = (idx & 0x1) << 1;
		break;
	case aarch64::qv_4s:
		sloejit_assert(idx < 4);
		opc = 0b100;
		q = (idx & 0x2) >> 1;
		s = idx & 0x1;
		sz = 0b00;
		break;
	case aarch64::qv_2d:
		sloejit_assert(idx < 2);
		opc = 0b100;
		q = idx;
		s = 0;
		sz = 0b01;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (opc << 13) | (s << 12) | (sz << 10) | (rn << 5) | rt);
}

template <uint32_t op>
static void create_bin_ld1x_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert((imm << (32 - 4)) >> (32 - 4) == imm);
	// ld1b_z_p_bi.xml
	// ld1h_z_p_bi.xml
	// ld1w_z_p_bi.xml
	// ld1d_z_p_bi.xml
	uint32_t base = 0b1010010'0000'0'0000'101'000'00000'00000u;
	//                        ~op~   imm4     Pg~ ~Rn~~ ~Rt~~
	//                        21-^   16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | ((static_cast<uint32_t>(imm) & 0xfu) << 16) | (pg << 10) | (rn << 5) |
	            rt);
}

static void create_bin_ld1b_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_b: create_bin_ld1x_zpri<0b0000>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_h: create_bin_ld1x_zpri<0b0001>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_s: create_bin_ld1x_zpri<0b0010>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_d: create_bin_ld1x_zpri<0b0011>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_q: sloejit_assert(false);
	}
}

static void create_bin_ld1h_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_h: create_bin_ld1x_zpri<0b0101>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_s: create_bin_ld1x_zpri<0b0110>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_d: create_bin_ld1x_zpri<0b0111>(bv, rt, pg, rn, imm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1w_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_s: create_bin_ld1x_zpri<0b1010>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_d: create_bin_ld1x_zpri<0b1011>(bv, rt, pg, rn, imm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1d_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d: create_bin_ld1x_zpri<0b1111>(bv, rt, pg, rn, imm); break;
	default: sloejit_assert(false);
	}
}

template <uint32_t op>
static void create_bin_ld1x_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not allowed
	// ld1b_z_p_br.xml
	// ld1h_z_p_br.xml
	// ld1w_z_p_br.xml
	// ld1d_z_p_br.xml
	uint32_t base = 0b1010010'0000'00000'010'000'00000'00000u;
	//                        ~op~ ~Rm~~     Pg~ ~Rn~~ ~Rt~~
	//                        21-^  16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | (rm << 16) | (pg << 10) | (rn << 5) | rt);
}

static void create_bin_ld1b_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_b: create_bin_ld1x_zprr<0b0000>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_h: create_bin_ld1x_zprr<0b0001>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_s: create_bin_ld1x_zprr<0b0010>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_d: create_bin_ld1x_zprr<0b0011>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_q: sloejit_assert(false);
	}
}

static void create_bin_ld1h_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_h: create_bin_ld1x_zprr<0b0101>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_s: create_bin_ld1x_zprr<0b0110>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_d: create_bin_ld1x_zprr<0b0111>(bv, rt, pg, rn, rm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1w_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_s: create_bin_ld1x_zprr<0b1010>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_d: create_bin_ld1x_zprr<0b1011>(bv, rt, pg, rn, rm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1d_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d: create_bin_ld1x_zprr<0b1111>(bv, rt, pg, rn, rm); break;
	default: sloejit_assert(false);
	}
}

template <uint32_t op>
static void create_bin_ld1x_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// ld1b_z_p_bz.xml
	// ld1h_z_p_bz.xml
	// ld1w_z_p_bz.xml
	// ld1d_z_p_bz.xml
	// note: we only care about the scaled and unscaled offset versions for now (i.e. not the unpacked ones)
	uint32_t base = 0b1100010'0000'00000'110'000'00000'00000u;
	//                        ~op~ ~Rm~~     Pg~ ~Rn~~ ~Rt~~
	//                        21-^  16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | (rm << 16) | (pg << 10) | (rn << 5) | rt);
}

template <uint32_t op>
static void create_bin_ld1x_sxtw_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// ld1b_z_p_bz.xml
	// ld1h_z_p_bz.xml
	// ld1w_z_p_bz.xml
	// ld1d_z_p_bz.xml
	// note: we only care about the scaled offset version for now (i.e. not the unpacked ones)
	//                       xs-v
	uint32_t base = 0b1000010'0010'00000'010'000'00000'00000u;
	//                        ~op~ ~Rm~~     Pg~ ~Rn~~ ~Rt~~
	//                        21-^  16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | (rm << 16) | (pg << 10) | (rn << 5) | rt);
}

static void create_bin_ld1b_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
                sloejit_assert(sh == 0);
		create_bin_ld1x_zprz<0b0010>(bv, rt, pg, rn, rm);
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1h_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
                sloejit_assert(sh == 0 || sh == 1);
		if (sh == 0) {
			create_bin_ld1x_zprz<0b0110>(bv, rt, pg, rn, rm);
		}
		if (sh == 1) {
			create_bin_ld1x_zprz<0b0111>(bv, rt, pg, rn, rm);
		}
		break;
	case aarch64::zv_s:
	        sloejit_assert(sh == 1);
		create_bin_ld1x_sxtw_zprz<0b0111>(bv, rt, pg, rn, rm);
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1w_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
	        sloejit_assert(sh == 0 || sh == 2);
		if (sh == 0) {
			create_bin_ld1x_zprz<0b1010>(bv, rt, pg, rn, rm);
		}
		if (sh == 2) {
			create_bin_ld1x_zprz<0b1011>(bv, rt, pg, rn, rm);
		}
		break;
	case aarch64::zv_s:
	        sloejit_assert(sh == 2);
		create_bin_ld1x_sxtw_zprz<0b1011>(bv, rt, pg, rn, rm);
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_ld1d_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
                sloejit_assert(sh == 0 || sh == 3);
		if (sh == 0) {
			create_bin_ld1x_zprz<0b1110>(bv, rt, pg, rn, rm);
		}
		if (sh == 3) {
			create_bin_ld1x_zprz<0b1111>(bv, rt, pg, rn, rm);
		}
		break;
	default: sloejit_assert(false);
	}
}

template <uint32_t op, int32_t shift>
static void create_bin_ld1rx_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert((imm << (32 - (6 + shift))) >> (32 - (6 + shift)) == imm);
	sloejit_assert((imm >> shift) << shift == imm);
	imm = (imm >> shift) & 0x3fu;
	// ld1rb_z_p_bi.xml
	// ld1rh_z_p_bi.xml
	// ld1rw_z_p_bi.xml
	// ld1rd_z_p_bi.xml
	uint32_t base = 0b1000010'00'1'000000'1'00'000'00000'00000u;
	//                        op   ~imm6~   op Pg~ ~Rn~~ ~Rt~~
	//                      23-^     16-^     10-^   5-^   0-^
	bv.push_u32(base | (op << 23) | (imm << 16) | (op << 13) | (pg << 10) | (rn << 5) | rt);
}

static void create_bin_ld1rb_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm) {
	create_bin_ld1rx_zpri<0b00, 0>(bv, rt, pg, rn, imm);
}

static void create_bin_ld1rh_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm) {
	create_bin_ld1rx_zpri<0b01, 1>(bv, rt, pg, rn, imm);
}

static void create_bin_ld1rw_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm) {
	create_bin_ld1rx_zpri<0b10, 2>(bv, rt, pg, rn, imm);
}

static void create_bin_ld1rd_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm) {
	create_bin_ld1rx_zpri<0b11, 3>(bv, rt, pg, rn, imm);
}

template <uint32_t op, int32_t shift>
static void create_bin_ld1rqx_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert((imm << (32 - (4 + shift))) >> (32 - (4 + shift)) == imm);
	sloejit_assert((imm >> shift) << shift == imm);
	imm = (imm >> shift) & 0xfu;
	// ld1rqb_z_p_bi.xml
	// ld1rqh_z_p_bi.xml
	// ld1rqw_z_p_bi.xml
	// ld1rqd_z_p_bi.xml
	uint32_t base = 0b1010010'00'000'0000'001'000'00000'00000u;
	//                        op     imm4     Pg~ ~Rn~~ ~Rt~~
	//                      23-^     16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 23) | (imm << 16) | (pg << 10) | (rn << 5) | rt);
}

static void create_bin_ld1rqb_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld1rqx_zpri<0b00, 4>(bv, rt, pg, rn, imm);
}

static void create_bin_ld1rqh_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld1rqx_zpri<0b01, 4>(bv, rt, pg, rn, imm);
}

static void create_bin_ld1rqw_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld1rqx_zpri<0b10, 4>(bv, rt, pg, rn, imm);
}

static void create_bin_ld1rqd_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld1rqx_zpri<0b11, 4>(bv, rt, pg, rn, imm);
}

template<uint32_t op, uint32_t msz>
void create_bin_ldst1x_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t off, uint32_t pg, uint32_t rn, uint32_t rm) {
	static_assert(msz < 5);
	static_assert(op < 2);
	sloejit_assert(za == 0);
	sloejit_assert(tile < (1 << msz));
	sloejit_assert(v < 2);
	sloejit_assert(rs >= 12 && rs <= 15);
	sloejit_assert(off < (1 << (4 - msz)));
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);

	// ld1b_za_p_rrr.xml
	// ld1h_za_p_rrr.xml
	// ld1w_za_p_rrr.xml
	// ld1d_za_p_rrr.xml
	// ld1q_za_p_rrr.xml
	// st1b_za_p_rrr.xml
	// st1h_za_p_rrr.xml
	// st1w_za_p_rrr.xml
	// st1d_za_p_rrr.xml
	// st1q_za_p_rrr.xml
	uint32_t base = 0b11100000'00'0'00000'0'00'000'00000'00000;
	//                        msz   ~~Rm~ V Rs ~Pg ~~Rn~
	//                       22-^    16-^  13^ 10^   5-^
	bv.push_u32(base | ((msz == 4 ? 7 : msz) << 22) | (op << 21) | (rm << 16) | (v << 15) | ((rs - 12) << 13) | (pg << 10) | (rn << 5) | (tile << (4 - msz)) | off);
}

static void create_bin_ld1b_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t off4, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<0, 0b00>(bv, za, tile, v, rs, off4, pg, rn, rm);
}

static void create_bin_ld1h_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t off3, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<0, 0b01>(bv, za, tile, v, rs, off3, pg, rn, rm);
}

static void create_bin_ld1w_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t off3, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<0, 0b10>(bv, za, tile, v, rs, off3, pg, rn, rm);
}

static void create_bin_ld1d_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<0, 0b11>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

static void create_bin_ld1q_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<0, 0b100>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_ld2x_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % 2 == 0 && imm >= -16 && imm <= 14);
	int32_t imm4 = (imm / 2) & 0b1111u;
	// ld2b_z_p_bi.xml
	// ld2h_z_p_bi.xml
	// ld2w_z_p_bi.xml
	// ld2d_z_p_bi.xml
	uint32_t base = 0b1010010'00'00'0'0000'111'000'00000'00000;
	//                       msz opc  imm4      Pg    Rn    Zt
	//                      23-^  ^-21 16^    10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_ld2x_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// ld2b_z_p_br.xml
	// ld2h_z_p_br.xml
	// ld2w_z_p_br.xml
	// ld2d_z_p_br.xml
	uint32_t base = 0b1010010'00'00'00000'110'000'00000'00000;
	//                       msz opc   Rm      Pg    Rn    Zt
	//                      23-^  ^-21  ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_ld2b_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld2x_zzpri<0b00u, 0b01u>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_ld2b_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld2x_zzprr<0b00u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_ld2h_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld2x_zzpri<0b01u, 0b01u>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_ld2h_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld2x_zzprr<0b01u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_ld2w_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld2x_zzpri<0b10u, 0b01u>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_ld2w_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld2x_zzprr<0b10u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_ld2d_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld2x_zzpri<0b11u, 0b01u>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_ld2d_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld2x_zzprr<0b11u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_ld2q_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % 2 == 0 && imm >= -16 && imm <= 14);
	int32_t imm4 = (imm / 2) & 0b1111u;
	// ld2q_z_p_bi.xml
	uint32_t base = 0b1010010'0000'1'0000'111'000'00000'00000;
	//                        num   imm4     Pg~ ~Rn~~ ~Zt~~
	//                        24-^   16-^    10-^   5-^   0-^
	bv.push_u32(base | (0b01u << 23) | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_ld2q_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// ld2q_z_p_br.xml
	uint32_t base = 0b1010010'0001'00000'100'000'00000'00000;
	//                        num   ~Rm~~    Pg~ ~Rn~~ ~Zt~~
	//                        24-^   16-^    10-^   5-^   0-^
	bv.push_u32(base | (0b01u << 23) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_ld3x_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % 3 == 0 && imm >= -24 && imm <= 21);
	int32_t imm4 = (imm / 3) & 0b1111u;
	// ld3b_z_p_bi.xml
	// ld3h_z_p_bi.xml
	// ld3w_z_p_bi.xml
	// ld3d_z_p_bi.xml
	uint32_t base = 0b1010010'00'00'0'0000'111'000'00000'00000;
	//                       msz opc  imm4     Pg~ ~Rn~~ ~Zt~~
	//                      23-^  ^-21   ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_ld3x_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// ld3b_z_p_br.xml
	// ld3h_z_p_br.xml
	// ld3w_z_p_br.xml
	// ld3d_z_p_br.xml
	uint32_t base = 0b1010010'00'00'00000'110'000'00000'00000;
	//                       msz opc~Rm~~     Pg~ ~Rn~~ ~Zt~~
	//                      23-^  ^-21  ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_ld3b_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld3x_zzzpri<0b00u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, imm);
}

static void create_bin_ld3b_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld3x_zzzprr<0b00u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, rm);
}

static void create_bin_ld3h_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld3x_zzzpri<0b01u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, imm);
}

static void create_bin_ld3h_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld3x_zzzprr<0b01u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, rm);
}

static void create_bin_ld3w_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld3x_zzzpri<0b10u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, imm);
}

static void create_bin_ld3w_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld3x_zzzprr<0b10u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, rm);
}

static void create_bin_ld3d_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld3x_zzzpri<0b11u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, imm);
}

static void create_bin_ld3d_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld3x_zzzprr<0b11u, 0b10u>(bv, zt1, zt2, zt3, pg, rn, rm);
}

static void create_bin_ld3q_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % 3 == 0 && imm >= -24 && imm <= 21);
	int32_t imm4 = (imm / 3) & 0b1111u;
	// ld3q_z_p_bi.xml
	uint32_t base = 0b101001010001'0000'111'000'00000'00000;
	//                             imm4     Pg~ ~Rn~~ ~Zt~~
	//                             16-^    10-^   5-^   0-^
	bv.push_u32(base | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_ld3q_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// ld3q_z_p_br.xml
	uint32_t base = 0b10100101001'00000'100'000'00000'00000;
	//                            ~Rm~~    Pg~ ~Rn~~ ~Zt~~
	//                             16-^    10-^   5-^   0-^
	bv.push_u32(base | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_ld4x_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % 4 == 0 && imm >= -32 && imm <= 28);
	int32_t imm4 = (imm / 4) & 0b1111u;
	// ld4b_z_p_bi.xml
	// ld4h_z_p_bi.xml
	// ld4w_z_p_bi.xml
	// ld4d_z_p_bi.xml
	uint32_t base = 0b1010010'00'11'0'0000'111'000'00000'00000;
	//                       msz opc  imm4     Pg~ ~Rn~~ ~Zt~~
	//                      23-^  ^-21   ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_ld4x_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// ld4b_z_p_br.xml
	// ld4h_z_p_br.xml
	// ld4w_z_p_br.xml
	// ld4d_z_p_br.xml
	uint32_t base = 0b1010010'00'00'00000'110'000'00000'00000;
	//                       msz opc~Rm~~     Pg~ ~Rn~~ ~Zt~~
	//                      23-^  ^-21  ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_ld4b_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld4x_zzzzpri<0b00u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, imm);
}

static void create_bin_ld4b_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld4x_zzzzprr<0b00u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, rm);
}

static void create_bin_ld4h_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld4x_zzzzpri<0b01u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, imm);
}

static void create_bin_ld4h_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld4x_zzzzprr<0b01u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, rm);
}

static void create_bin_ld4w_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld4x_zzzzpri<0b10u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, imm);
}

static void create_bin_ld4w_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld4x_zzzzprr<0b10u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, rm);
}

static void create_bin_ld4d_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_ld4x_zzzzpri<0b11u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, imm);
}

static void create_bin_ld4d_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ld4x_zzzzprr<0b11u, 0b11u>(bv, zt1, zt2, zt3, zt4, pg, rn, rm);
}

static void create_bin_ld4q_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % 4 == 0 && imm >= -32 && imm <= 28);
	int32_t imm4 = (imm / 4) & 0b1111u;
	// ld4q_z_p_bi.xml
	uint32_t base = 0b101001011001'0000'111'000'00000'00000;
	//                             imm4     Pg~ ~Rn~~ ~Zt~~
	//                             16-^    10-^   5-^   0-^
	bv.push_u32(base | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_ld4q_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// ld4q_z_p_br.xml
	//                        num   ~Rm~~    Pg~ ~Rn~~ ~Zt~~
	//                        24-^   16-^    10-^   5-^   0-^
	uint32_t base = 0b1010010'0001'00000'100'000'00000'00000;
	bv.push_u32(base | (0b11u << 23) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc, uint32_t nreg>
static void create_bin_stx_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                 uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % static_cast<int32_t>(nreg) == 0 && imm >= -8 * static_cast<int32_t>(nreg) &&
	               imm <= 7 * static_cast<int32_t>(nreg));
	int32_t imm4 = (imm / static_cast<int32_t>(nreg)) & 0b1111u;
	// st2b_z_p_bi.xml
	// st2h_z_p_bi.xml
	// st2w_z_p_bi.xml
	// st2d_z_p_bi.xml
	// st3b_z_p_bi.xml
	// st3h_z_p_bi.xml
	// st3w_z_p_bi.xml
	// st3d_z_p_bi.xml
	// st4b_z_p_bi.xml
	// st4h_z_p_bi.xml
	// st4w_z_p_bi.xml
	// st4d_z_p_bi.xml
	uint32_t base = 0b1110010'00'00'1'0000'111'000'00000'00000;
	//                       msz opc  imm4     Pg~ ~Rn~~ ~Zt~~
	//                      23-^  ^-21   ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t msz, uint32_t opc>
static void create_bin_stx_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                 uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// st2b_z_p_br.xml
	// st2h_z_p_br.xml
	// st2w_z_p_br.xml
	// st2d_z_p_br.xml
	// st3b_z_p_br.xml
	// st3h_z_p_br.xml
	// st3w_z_p_br.xml
	// st3d_z_p_br.xml
	// st4b_z_p_br.xml
	// st4h_z_p_br.xml
	// st4w_z_p_br.xml
	// st4d_z_p_br.xml
	uint32_t base = 0b1110010'00'00'00000'011'000'00000'00000;
	//                       msz opc~Rm~~     Pg~ ~Rn~~ ~Zt~~
	//                      23-^  ^-21  ^-16 10-^   5-^   0-^
	bv.push_u32(base | (msz << 23) | (opc << 21) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_st2b_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_stx_zzpri<0b00u, 0b01u, 2>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st2b_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_stx_zzprr<0b00u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st2h_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_stx_zzpri<0b01u, 0b01u, 2>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st2h_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_stx_zzprr<0b01u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st2w_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_stx_zzpri<0b10u, 0b01u, 2>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st2w_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_stx_zzprr<0b10u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st2d_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_stx_zzpri<0b11u, 0b01u, 2>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st2d_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_stx_zzprr<0b11u, 0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st3b_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzpri<0b00u, 0b10u, 3>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st3b_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzprr<0b00u, 0b10u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st3h_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzpri<0b01u, 0b10u, 3>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st3h_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzprr<0b01u, 0b10u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st3w_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzpri<0b10u, 0b10u, 3>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st3w_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzprr<0b10u, 0b10u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st3d_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzpri<0b11u, 0b10u, 3>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st3d_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stx_zzprr<0b11u, 0b10u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st4b_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzpri<0b00u, 0b11u, 4>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st4b_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzprr<0b00u, 0b11u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st4h_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzpri<0b01u, 0b11u, 4>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st4h_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzprr<0b01u, 0b11u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st4w_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzpri<0b10u, 0b11u, 4>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st4w_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzprr<0b10u, 0b11u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st4d_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzpri<0b11u, 0b11u, 4>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st4d_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stx_zzprr<0b11u, 0b11u>(bv, zt1, zt2, pg, rn, rm);
}

template <uint32_t num, uint32_t nreg>
static void create_bin_stqx_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(imm % static_cast<int32_t>(nreg) == 0 && imm >= -8 * static_cast<int32_t>(nreg) &&
	               imm <= 7 * static_cast<int32_t>(nreg));
	int32_t imm4 = (imm / static_cast<int32_t>(nreg)) & 0b1111u;
	// st2q_z_p_bi.xml
	// st3q_z_p_bi.xml
	// st4q_z_p_bi.xml
	uint32_t base = 0b1110010'0'00'00'0000'000'000'00000'00000;
	//                        num  imm4     Pg~ ~Rn~~ ~Zt~~
	//                        22-^ 16-^    10-^   5-^   0-^
	bv.push_u32(base | (num << 22) | (imm4 << 16) | (pg << 10) | (rn << 5) | zt1);
}

template <uint32_t num>
static void create_bin_stqx_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt1 < 32);
	sloejit_assert(zt2 < 32);
	sloejit_assert((zt1 + 1) % 32 == zt2);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not permitted
	// st2q_z_p_br.xml
	// st3q_z_p_br.xml
	// st4q_z_p_br.xml
	uint32_t base = 0b1110010'0'00'1'00000'000'000'00000'00000;
	//                        num   ~Rm~~    Pg~ ~Rn~~ ~Zt~~
	//                        22-^   16-^    10-^   5-^   0-^
	bv.push_u32(base | (num << 22) | (rm << 16) | (pg << 10) | (rn << 5) | zt1);
}

static void create_bin_st2q_zzpri(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, int32_t imm) {
	create_bin_stqx_zzpri<0b01u, 2>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st2q_zzprr(bytevector &bv, uint32_t zt1, uint32_t zt2,
                                  uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_stqx_zzprr<0b01u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st3q_zzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stqx_zzpri<0b10u, 3>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st3q_zzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                   uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	create_bin_stqx_zzprr<0b10u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st4q_zzzzpri(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stqx_zzpri<0b11u, 4>(bv, zt1, zt2, pg, rn, imm);
}

static void create_bin_st4q_zzzzprr(bytevector &bv, uint32_t zt1, uint32_t zt2, uint32_t zt3,
                                    uint32_t zt4, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(zt3 < 32);
	sloejit_assert(zt4 < 32);
	sloejit_assert((zt1 + 2) % 32 == zt3);
	sloejit_assert((zt1 + 3) % 32 == zt4);
	create_bin_stqx_zzprr<0b11u>(bv, zt1, zt2, pg, rn, rm);
}

static void create_bin_st1b_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<1, 0b00>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

static void create_bin_st1h_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<1, 0b01>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

static void create_bin_st1w_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<1, 0b10>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

static void create_bin_st1d_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<1, 0b11>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

static void create_bin_st1q_alorlprr(bytevector &bv, uint32_t za, uint32_t tile, uint32_t v, uint32_t rs, uint32_t o1, uint32_t pg, uint32_t rn, uint32_t rm) {
	create_bin_ldst1x_alorlprr<1, 0b100>(bv, za, tile, v, rs, o1, pg, rn, rm);
}

template <uint32_t op>
static void create_bin_st1x_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, int32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert((imm << (32 - 4)) >> (32 - 4) == imm);
	// st1b_z_p_bi.xml
	// st1h_z_p_bi.xml
	// st1w_z_p_bi.xml
	// st1d_z_p_bi.xml
	uint32_t base = 0b1110010'0000'0'0000'111'000'00000'00000u;
	//                        ~op~   imm4     Pg~ ~Rn~~ ~Rt~~
	//                        21-^   16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | ((static_cast<uint32_t>(imm) & 0xfu) << 16) | (pg << 10) | (rn << 5) |
	            rt);
}

static void create_bin_st1b_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_b: create_bin_st1x_zpri<0b0000>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_h: create_bin_st1x_zpri<0b0001>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_s: create_bin_st1x_zpri<0b0010>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_d: create_bin_st1x_zpri<0b0011>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_q: sloejit_assert(false);
	}
}

static void create_bin_st1h_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_h: create_bin_st1x_zpri<0b0101>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_s: create_bin_st1x_zpri<0b0110>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_d: create_bin_st1x_zpri<0b0111>(bv, rt, pg, rn, imm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_st1w_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_s: create_bin_st1x_zpri<0b1010>(bv, rt, pg, rn, imm); break;
	case aarch64::zv_d: create_bin_st1x_zpri<0b1011>(bv, rt, pg, rn, imm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_st1d_zpri(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t imm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d: create_bin_st1x_zpri<0b1111>(bv, rt, pg, rn, imm); break;
	default: sloejit_assert(false);
	}
}

template <uint32_t op>
static void create_bin_st1x_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 31); // 0b11111 not allowed
	// st1b_z_p_br.xml
	// st1h_z_p_br.xml
	// st1w_z_p_br.xml
	// st1d_z_p_br.xml
	uint32_t base = 0b1110010'0000'00000'010'000'00000'00000u;
	//                        ~op~ ~Rm~~     Pg~ ~Rn~~ ~Rt~~
	//                        21-^  16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | (rm << 16) | (pg << 10) | (rn << 5) | rt);
}

static void create_bin_st1b_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_b: create_bin_st1x_zprr<0b0000>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_h: create_bin_st1x_zprr<0b0001>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_s: create_bin_st1x_zprr<0b0010>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_d: create_bin_st1x_zprr<0b0011>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_q: sloejit_assert(false);
	}
}

static void create_bin_st1h_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_h: create_bin_st1x_zprr<0b0101>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_s: create_bin_st1x_zprr<0b0110>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_d: create_bin_st1x_zprr<0b0111>(bv, rt, pg, rn, rm); break;
	default: sloejit_assert(false);
	}
}

static void create_bin_st1w_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_s: create_bin_st1x_zprr<0b1010>(bv, rt, pg, rn, rm); break;
	case aarch64::zv_d: create_bin_st1x_zprr<0b1011>(bv, rt, pg, rn, rm); break;
        default: sloejit_assert(false);
	}
}

static void create_bin_st1d_zprr(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d: create_bin_st1x_zprr<0b1111>(bv, rt, pg, rn, rm); break;
	default: sloejit_assert(false);
	}
}

template <uint32_t op>
static void create_bin_st1x_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// st1b_z_p_bz.xml
	// st1h_z_p_bz.xml
	// st1w_z_p_bz.xml
	// st1d_z_p_bz.xml
	// note: we only care about the scaled and unscaled offset versions for now (i.e. not the unpacked ones)
	uint32_t base = 0b1110010'0000'00000'101'000'00000'00000u;
	//                        ~op~ ~Rm~~     Pg~ ~Rn~~ ~Rt~~
	//                        21-^  16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | (rm << 16) | (pg << 10) | (rn << 5) | rt);
}

template <uint32_t op>
static void create_bin_st1x_sxtw_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm) {
	sloejit_assert(rt < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// st1b_z_p_bz.xml
	// st1h_z_p_bz.xml
	// st1w_z_p_bz.xml
	// st1d_z_p_bz.xml
	// note: we only care about the scaled offset version for now (i.e. not the unpacked ones)
	uint32_t base = 0b1110010'0000'00000'110'000'00000'00000u;
	//                        ~op~ ~Rm~~     Pg~ ~Rn~~ ~Rt~~
	//                        21-^  16-^    10-^   5-^   0-^
	bv.push_u32(base | (op << 21) | (rm << 16) | (pg << 10) | (rn << 5) | rt);
}

static void create_bin_st1b_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
	        sloejit_assert(sh == 0);
		create_bin_st1x_zprz<0b0000>(bv, rt, pg, rn, rm);
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_st1h_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
	        sloejit_assert(sh == 0 || sh == 1);
		if (sh == 0) {
			create_bin_st1x_zprz<0b0100>(bv, rt, pg, rn, rm);
		}
		if (sh == 1) {
			create_bin_st1x_zprz<0b0101>(bv, rt, pg, rn, rm);
		}
		break;
	case aarch64::zv_s:
	        sloejit_assert(sh == 1);
		create_bin_st1x_sxtw_zprz<0b0111>(bv, rt, pg, rn, rm);
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_st1w_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
	        sloejit_assert(sh == 2 || sh == 0);
		if (sh == 0) {
			create_bin_st1x_zprz<0b1000>(bv, rt, pg, rn, rm);
		}
		if (sh == 2) {
			create_bin_st1x_zprz<0b1001>(bv, rt, pg, rn, rm);
                }
		break;
	case aarch64::zv_s:
		sloejit_assert (sh == 2);
		create_bin_st1x_sxtw_zprz<0b1011>(bv, rt, pg, rn, rm);
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_st1d_zprz(bytevector &bv, uint32_t rt, uint32_t pg, uint32_t rn, uint32_t rm, int sh,
                                 aarch64::z_type_variant zv) {
	switch (zv) {
	case aarch64::zv_d:
	        sloejit_assert(sh == 0 || sh == 3);
		if (sh == 0) {
			create_bin_st1x_zprz<0b1100>(bv, rt, pg, rn, rm);
		}
		if (sh == 3) {
			create_bin_st1x_zprz<0b1101>(bv, rt, pg, rn, rm);
		}
		break;
	default: sloejit_assert(false);
	}
}

static void create_bin_cbnz_ri(bytevector &bv, uint32_t rt, int32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert((imm & 0x3) == 0);
	imm >>= 2;
	sloejit_assert((imm << (32 - 19)) >> (32 - 19) == imm);
	// cbnz.xml
	uint32_t base = 0b10110101'0000000000000000000'00000u;
	//                         ~~~~~~~~imm~~~~~~~~ ~Rt~~
	//                                         5-^   0-^
	bv.push_u32(base | ((static_cast<uint32_t>(imm) & 0x7ffffu) << 5) | rt);
}

static void create_bin_cbz_ri(bytevector &bv, uint32_t rt, int32_t imm) {
	sloejit_assert(rt < 32);
	sloejit_assert((imm & 0x3) == 0);
	imm >>= 2;
	sloejit_assert((imm << (32 - 19)) >> (32 - 19) == imm);
	// cbz.xml
	uint32_t base = 0b10110100'0000000000000000000'00000u;
	//                         ~~~~~~~~imm~~~~~~~~ ~Rt~~
	//                                         5-^   0-^
	bv.push_u32(base | ((static_cast<uint32_t>(imm) & 0x7ffffu) << 5) | rt);
}

static void create_bin_blr_r(bytevector &bv, uint32_t rn) {
	sloejit_assert(rn < 32);
	// blr.xml
	uint32_t base = 0b1101011000111111000000'00000'00000u;
	//                                       ~~Rn~
	//                                         5-^
	bv.push_u32(base | rn << 5);
}

template <uint32_t op>
static void create_bin_bx_i(bytevector &bv, int32_t imm) {
	sloejit_assert((imm & 0x3) == 0);
	imm >>= 2;
	sloejit_assert((imm << (32 - 26)) >> (32 - 26) == imm);
	// b_uncond.xml
	uint32_t base = 0b0'00101'00000000000000000000000000u;
	//               op       ~~~~~~~~~~imm26~~~~~~~~~~~
	//             31-^                             0-^
	bv.push_u32(base | (op << 31) | (static_cast<uint32_t>(imm) & 0x3ffffffu));
}

static void create_bin_b_i(bytevector &bv, int32_t imm) {
	create_bin_bx_i<0>(bv, imm);
}

static void create_bin_bl_i(bytevector &bv, int32_t imm) {
	create_bin_bx_i<1>(bv, imm);
}

static void create_bin_b_cond_i(bytevector &bv, uint32_t cond, int32_t imm) {
	sloejit_assert((imm & 0x3) == 0);
	sloejit_assert((cond & 0xfu) == cond);
	imm >>= 2;
	sloejit_assert((imm << (32 - 19)) >> (32 - 19) == imm);
	// b_cond.xml
	uint32_t base = 0b01010100'0000000000000000000'0'0000u;
	//                         ~~~~~~~imm19~~~~~~~   cond
	//                                         5-^    0-^
	bv.push_u32(base | ((static_cast<uint32_t>(imm) & 0x7ffffu) << 5) | cond);
}

static void create_bin_ret_(bytevector &bv) {
	uint32_t rn = 30;
	sloejit_assert(rn < 32);
	// ret.xml
	uint32_t base = 0b1101011001011111000000'00000'00000u;
	//                                       ~Rn~~
	//                                         5-^
	bv.push_u32(base | (rn << 5));
}

static void create_bin_smstart_i(bytevector &bv, uint32_t i) {
	// smstart_msr_imm.xml
	uint32_t base = 0b1101010100000'011'0100'0001'011'11111;
	//                              op1      Crm  op2
	//                                       9-^
	sloejit_assert(i >=1 && i <= 3);
	bv.push_u32(base | (i << 9));
}

static void create_bin_smstop_i(bytevector &bv, uint32_t i) {
	// smstop_msr_imm.xml
	uint32_t base = 0b1101010100000'011'0100'0000'011'11111;
	//                              op1      Crm  op2
	//                                       9-^
	sloejit_assert(i >=1 && i <= 3);
	bv.push_u32(base | (i << 9));
}

static void create_bin_sqneg_zpz(bytevector &bv, uint32_t rd, uint32_t pg, uint32_t rn,
                                 aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(pg < 8);
	sloejit_assert(rn < 32);
	// sqneg_z_p_z.xml#iclass_merging
	uint32_t base = 0b01000100'00'001001101'000'00000'00000u;
	//                         sz           Pg~ ~Zn~~ ~Zd~~
	//                       22-^          10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (pg << 10) | (rn << 5) | rd);
}

static void create_bin_sqneg_qq(bytevector &bv, uint32_t rd, uint32_t rn, aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	// sqneg_advsimd.xml
	uint32_t base = 0b0'0'101110'00'100000011110'00000'00000u;
	//                  Q        sz              ~Rn~~ ~Rd~~
	//               30-^      22-^                5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rn << 5) | rd);
}

template <uint32_t op>
static void create_bin_sqaddsub_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqadd_z_zz.xml
	// sqsub_z_zz.xml
	uint32_t base = 0b00000100'00'1'00000'0001'0'0'00000'00000u;
	//                         sz   ~Zm~~     op   ~Zn~~ ~Zd~~
	//                       22-^    16-^   11-^     5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 11) | (rn << 5) | rd);
}

static void create_bin_sqadd_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	create_bin_sqaddsub_zzz<0>(bv, rd, rn, rm, zv);
}

static void create_bin_sqsub_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::z_type_variant zv) {
	create_bin_sqaddsub_zzz<1>(bv, rd, rn, rm, zv);
}

template <uint32_t op>
static void create_bin_sqaddsub_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqadd_advsimd.xml
	// sqsub_advsimd.xml
	uint32_t base = 0b0'0'001110'00'1'00000'00'0'011'00000'00000u;
	//                  Q        sz   ~Rm~~   op     ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^ 13-^       5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		sz = 0b00;
		break;
	case aarch64::qv_16b:
		q = 1;
		sz = 0b00;
		break;
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	case aarch64::qv_2d:
		q = 1;
		sz = 0b11;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (op << 13) | (rn << 5) | rd);
}

static void create_bin_sqadd_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant qv) {
	create_bin_sqaddsub_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_sqsub_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant qv) {
	create_bin_sqaddsub_qqq<1>(bv, rd, rn, rm, qv);
}

template <uint32_t op>
static void create_bin_sqrdmlash_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm,
                                      unsigned lane, aarch64::z_type_variant zv) {
	sloejit_assert(rda < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmlah_z_zzzi.xml
	// sqrdmlsh_z_zzzi.xml
	uint32_t base = 0b01000100'0'0'1'00'000'00010'0'00000'00000u;
	//                        sz h   l~ Zm~      op ~Zn~~ ~Zda~
	//                        22-^ 19-^   ^-16 10-^   5-^   0-^
	uint32_t sz = 0, h = 0, l = 0;
	switch (zv) {
	case aarch64::zv_h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 8);
		sz = 0b0;
		h = lane >> 2;
		l = lane & 0b11;
		break;
	case aarch64::zv_s:
		sloejit_assert(lane < 4);
		sloejit_assert(rm < 8);
		sz = 0b1;
		h = 0b0;
		l = lane;
		break;
	case aarch64::zv_d:
		sloejit_assert(lane < 2);
		sloejit_assert(rm < 16);
		sz = 0b1;
		h = 0b1;
		l = lane << 1;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 23) | (h << 22) | (l << 19) | (rm << 16) | (op << 10) | (rn << 5) | rda);
}

static void create_bin_sqrdmlah_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm,
                                     unsigned lane, aarch64::z_type_variant zv) {
	create_bin_sqrdmlash_zzzl<0>(bv, rda, rn, rm, lane, zv);
}

static void create_bin_sqrdmlsh_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm,
                                     unsigned lane, aarch64::z_type_variant zv) {
	create_bin_sqrdmlash_zzzl<1>(bv, rda, rn, rm, lane, zv);
}

template <uint32_t a, uint32_t s>
static void create_bin_sqrdmlash_qqql(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                      aarch64::q_type_variant qv) {
	sloejit_assert(rda < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmlah_advsimd_elt.xml
	// sqrdmlsh_advsimd_elt.xml
	// sqrdmulh_advsimd_elt.xml
	uint32_t base = 0b0'0'0'01111'00'0'0'0000'11'0'1'0'0'00000'00000u;
	//                  Q a       sz L M ~Rm~    S   H   ~Rn~~ ~Rd~~
	//               30-^ ^-29  22-^     16-^ 13-^11-^     5-^   0-^
	uint32_t q = 0, sz = 0, h = 0, m = 0, l = 0;
	switch (qv) {
	case aarch64::qv_4h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 16);
		q = 0;
		sz = 0b01;
		h = lane >> 2;
		l = (lane & 0b10) >> 1;
		m = lane & 0b1;
		break;
	case aarch64::qv_8h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 16);
		q = 1;
		sz = 0b01;
		h = lane >> 2;
		l = (lane & 0b10) >> 1;
		m = lane & 0b1;
		break;
	case aarch64::qv_2s:
		sloejit_assert(lane < 4);
		q = 0;
		sz = 0b10;
		h = lane >> 1;
		l = lane & 0b1;
		m = rm >> 4;
		break;
	case aarch64::qv_4s:
		sloejit_assert(lane < 4);
		q = 1;
		sz = 0b10;
		h = lane >> 1;
		l = lane & 0b1;
		m = rm >> 4;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (a << 29) | (sz << 22) | (l << 21) | (m << 20) |
	            ((rm & 0xf) << 16) | (s << 13) | (h << 11) | (rn << 5) | rda);
}

static void create_bin_sqrdmlah_qqql(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                     aarch64::q_type_variant qv) {
	create_bin_sqrdmlash_qqql<0b1, 0b0>(bv, rda, rn, rm, lane, qv);
}

static void create_bin_sqrdmlsh_qqql(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm, unsigned lane,
                                     aarch64::q_type_variant qv) {
	create_bin_sqrdmlash_qqql<0b1, 0b1>(bv, rda, rn, rm, lane, qv);
}

static void create_bin_sqrdmulh_zzzl(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm,
                                     unsigned lane, aarch64::z_type_variant zv) {
	sloejit_assert(rda < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmulh_z_zzi.xml
	uint32_t base = 0b01000100'0'0'1'00'000'111101'00000'00000u;
	//                        sz h   l~ Zm~        ~Zn~~ ~Zda~
	//                        22-^ 19-^   ^-16       5-^   0-^
	uint32_t sz = 0, h = 0, l = 0;
	switch (zv) {
	case aarch64::zv_h:
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 8);
		sz = 0b0;
		h = lane >> 2;
		l = lane & 0b11;
		break;
	case aarch64::zv_s:
		sloejit_assert(lane < 4);
		sloejit_assert(rm < 8);
		sz = 0b1;
		h = 0b0;
		l = lane;
		break;
	case aarch64::zv_d:
		sloejit_assert(lane < 2);
		sloejit_assert(rm < 16);
		sz = 0b1;
		h = 0b1;
		l = lane << 1;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 23) | (h << 22) | (l << 19) | (rm << 16) | (rn << 5) | rda);
}

static void create_bin_sqrdmulh_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm, unsigned lane,
                                     aarch64::q_type_variant qv) {
	create_bin_sqrdmlash_qqql<0b0, 0b0>(bv, rd, rn, rm, lane, qv);
}

template <uint32_t op>
static void create_bin_sqrdmlash_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                     aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmlah_z_zzz.xml
	// sqrdmlsh_z_zzz.xml
	uint32_t base = 0b01000100'00'0'00000'01110'0'00000'00000u;
	//                         sz   ~Zm~~      op ~Zn~~ ~Zd~~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 10) | (rn << 5) | rd);
}

static void create_bin_sqrdmlah_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    aarch64::z_type_variant zv) {
	create_bin_sqrdmlash_zzz<0>(bv, rd, rn, rm, zv);
}

static void create_bin_sqrdmlsh_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    aarch64::z_type_variant zv) {
	create_bin_sqrdmlash_zzz<1>(bv, rd, rn, rm, zv);
}

template <uint32_t s>
static void create_bin_sqrdmlash_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmlah_advsimd_vec.xml
	// sqrdmlsh_advsimd_vec.xml
	uint32_t base = 0b0'0'101110'00'0'00000'1000'0'1'00000'00000u;
	//                  Q        sz   ~Rm~~      S   ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^   11-^     5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (s << 11) | (rn << 5) | rd);
}

static void create_bin_sqrdmlah_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
								aarch64::q_type_variant qv) {
	create_bin_sqrdmlash_qqq<0>(bv, rd, rn, rm, qv);
}

static void create_bin_sqrdmlsh_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
								 aarch64::q_type_variant qv) {
	create_bin_sqrdmlash_qqq<1>(bv, rd, rn, rm, qv);
}

static void create_bin_sqrdmulh_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    aarch64::z_type_variant zv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmulh_z_zz.xml
	uint32_t base = 0b00000100'00'1'00000'011101'00000'00000u;
	//                         sz   ~Zm~~        ~Zn~~ ~Zd~~
	//                       22-^    16-^          5-^   0-^
	uint32_t sz = 0;
	switch (zv) {
	case aarch64::zv_b:
		sz = 0b00;
		break;
	case aarch64::zv_h:
		sz = 0b01;
		break;
	case aarch64::zv_s:
		sz = 0b10;
		break;
	case aarch64::zv_d:
		sz = 0b11;
		break;
	default:
		sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 22) | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_sqrdmulh_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// sqrdmulh_advsimd_vec.xml
	uint32_t base = 0b0'0'101110'00'1'00000'101101'00000'00000u;
	//                  Q        sz   ~Rm~~        ~Rn~~ ~Rd~~
	//               30-^      22-^    16-^          5-^   0-^
	uint32_t q = 0, sz = 0;
	switch (qv) {
	case aarch64::qv_4h:
		q = 0;
		sz = 0b01;
		break;
	case aarch64::qv_8h:
		q = 1;
		sz = 0b01;
		break;
	case aarch64::qv_2s:
		q = 0;
		sz = 0b10;
		break;
	case aarch64::qv_4s:
		q = 1;
		sz = 0b10;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (q << 30) | (sz << 22) | (rm << 16) | (rn << 5) | rd);
}

static void create_bin_sqrdcmlah_zzzi(bytevector &bv, uint32_t rda, uint32_t rn, uint32_t rm,
                                      uint32_t rot, aarch64::z_type_variant zv) {
	sloejit_assert(rda < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	sloejit_assert(rot == 0 || rot == 90 || rot == 180 || rot == 270);
	rot /= 90;
	// sqrdcmlah_z_zzzi.xml
	uint32_t base = 0b01000100'00'0'00000'0011'00'00000'00000u;
	//                         sz   ~Rm~~     rot ~Rn~~ ~Rda~
	//                       22-^    16-^    10-^   5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	bv.push_u32(base | (sz << 22) | (rm << 16) | (rot << 10) | (rn << 5) | rda);
}

template<uint32_t op>
static void create_bin_smlasl_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::q_type_variant to, aarch64::q_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// smull_advsimd_vec.xml
	// smlal_advsimd_vec.xml
	// smlsl_advsimd_vec.xml
	uint32_t base = 0b00001110'00'1'00000'1'00'000'00000'00000u;
	//                         sz   ~Rm~~   op     ~Rn~~ ~Rd~~
	//                       22-^    16-^ 13-^       5-^   0-^
	uint32_t sz = 0;
	switch (to) {
	case aarch64::qv_8h:
		sloejit_assert(from == aarch64::qv_8b);
		sz = 0b00;
		break;
	case aarch64::qv_4s:
		sloejit_assert(from == aarch64::qv_4h);
		sz = 0b01;
		break;
	case aarch64::qv_2d:
		sloejit_assert(from == aarch64::qv_2s);
		sz = 0b10;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 13) | (rn << 5) | rd);
}

template<uint32_t op>
static void create_bin_smlasl2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   aarch64::q_type_variant to, aarch64::q_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// smull_advsimd_vec.xml
	// smlal_advsimd_vec.xml
	// smlsl_advsimd_vec.xml
	uint32_t base = 0b01001110'00'1'00000'1'00'000'00000'00000u;
	//                         sz   ~Rm~~   op     ~Rn~~ ~Rd~~
	//                       22-^    16-^ 13-^       5-^   0-^
	uint32_t sz = 0;
	switch (to) {
	case aarch64::qv_8h:
		sloejit_assert(from == aarch64::qv_16b);
		sz = 0b00;
		break;
	case aarch64::qv_4s:
		sloejit_assert(from == aarch64::qv_8h);
		sz = 0b01;
		break;
	case aarch64::qv_2d:
		sloejit_assert(from == aarch64::qv_4s);
		sz = 0b10;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 22) | (rm << 16) | (op << 13) | (rn << 5) | rd);
}

static void create_bin_smlal_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl_qqq<0>(bv, rd, rn, rm, to, from);
}

static void create_bin_smlal2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl2_qqq<0>(bv, rd, rn, rm, to, from);
}

static void create_bin_smlsl_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl_qqq<1>(bv, rd, rn, rm, to, from);
}

static void create_bin_smlsl2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl2_qqq<1>(bv, rd, rn, rm, to, from);
}

static void create_bin_smull_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                 aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl_qqq<2>(bv, rd, rn, rm, to, from);
}

static void create_bin_smull2_qqq(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl2_qqq<2>(bv, rd, rn, rm, to, from);
}

template<uint32_t op>
static void create_bin_smlasl_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// smull_advsimd_elt.xml
	// smlal_advsimd_elt.xml
	// smlsl_advsimd_elt.xml
	//                         21-v      14-v
	uint32_t base = 0b00001111'00'0'0'0000'00'10'0'0'00000'00000u;
	//                         sz L M ~Rm~ op    H   ~Rn~~ ~Rd~~
	//                       22-^   ^-20 ^-16    ^-11  5-^   0-^
	uint32_t sz = 0, h = 0, l = 0, m = 0;
	switch (to) {
	case aarch64::qv_4s:
		sloejit_assert(from == aarch64::qv_4h);
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 16);
		sz = 0b01;
		h = lane >> 2;
		l = (lane >> 1) & 0b1;
		m = lane & 0b1;
		break;
	case aarch64::qv_2d:
		sloejit_assert(from == aarch64::qv_2s);
		sloejit_assert(lane < 4);
		sz = 0b10;
		h = lane >> 1;
		l = lane & 0b1;
		m = rm >> 4;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 22) | (l << 21) | (m << 20) | (rm << 16) | (op << 14) | (h << 11) | (rn << 5) | rd);
}

template<uint32_t op>
static void create_bin_smlasl2_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                    unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// smull_advsimd_elt.xml
	// smlal_advsimd_elt.xml
	// smlsl_advsimd_elt.xml
	//                         21-v      14-v
	uint32_t base = 0b01001111'00'0'0'0000'00'10'0'0'00000'00000u;
	//                         sz L M ~Rm~       H   ~Rn~~ ~Rd~~
	//                       22-^   ^-20 ^-16    ^-11  5-^   0-^
	uint32_t sz = 0, h = 0, l = 0, m = 0;
	switch (to) {
	case aarch64::qv_4s:
		sloejit_assert(from == aarch64::qv_8h);
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 16);
		sz = 0b01;
		h = lane >> 2;
		l = (lane >> 1) & 0b1;
		m = lane & 0b1;
		break;
	case aarch64::qv_2d:
		sloejit_assert(from == aarch64::qv_4s);
		sloejit_assert(lane < 4);
		sz = 0b10;
		h = lane >> 1;
		l = lane & 0b1;
		m = rm >> 4;
		break;
	default: sloejit_assert(false);
	}
	bv.push_u32(base | (sz << 22) | (l << 21) | (m << 20) | (rm << 16) | (op << 14) | (h << 11) | (rn << 5) | rd);
}

static void create_bin_smlal_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl_qqql<0>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smlal2_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl2_qqql<0>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smlsl_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl_qqql<1>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smlsl2_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl2_qqql<1>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smull_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl_qqql<2>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smull2_qqql(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::q_type_variant to, aarch64::q_type_variant from) {
	create_bin_smlasl2_qqql<2>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_sqrshrn_qqi(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t sh,
                                   aarch64::q_type_variant to, aarch64::q_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(sh > 0);
	// sqrshrn_advsimd.xml
	uint32_t base = 0b000011110'0000000'100111'00000'00000u;
	//                          ~imm~~~        ~Rn~~ ~Rd~~
	//                             16-^          5-^   0-^
	uint32_t imm = 0;
	uint32_t size = 0;
	switch (to) {
	case aarch64::qv_8b:
		sloejit_assert(from == aarch64::qv_8h);
		size = 8;
		break;
	case aarch64::qv_4h:
		sloejit_assert(from == aarch64::qv_4s);
		size = 16;
		break;
	case aarch64::qv_2s:
		sloejit_assert(from == aarch64::qv_2d);
		size = 32;
		break;
	default: sloejit_assert(false);
	}
	sloejit_assert(sh <= size);
	imm = 2 * size - sh;
	bv.push_u32(base | (imm << 16) | (rn << 5) | rd);
}

static void create_bin_sqrshrn2_qqi(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t sh,
                                    aarch64::q_type_variant to, aarch64::q_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(sh > 0);
	// sqrshrn_advsimd.xml
	uint32_t base = 0b010011110'0000000'100111'00000'00000u;
	//                          ~imm~~~        ~Rn~~ ~Rd~~
	//                             16-^          5-^   0-^
	uint32_t imm = 0;
	uint32_t size = 0;
	switch (to) {
	case aarch64::qv_16b:
		sloejit_assert(from == aarch64::qv_8h);
		size = 8;
		break;
	case aarch64::qv_8h:
		sloejit_assert(from == aarch64::qv_4s);
		size = 16;
		break;
	case aarch64::qv_4s:
		sloejit_assert(from == aarch64::qv_2d);
		size = 32;
		break;
	default: sloejit_assert(false);
	}
	sloejit_assert(sh <= size);
	imm = 2 * size - sh;
	bv.push_u32(base | (imm << 16) | (rn << 5) | rd);
}

template<int t>
static void create_bin_sqrshrnbt_zzi(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t sh,
                                     aarch64::z_type_variant to, aarch64::z_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(sh > 0);
	// sqrshrnb_z_zi.xml
	// sqrshrnt_z_zi.xml
	uint32_t base = 0b010001010'0'1'00'000'00101'0'00000'00000u;
	//                          h   l~imm3       T ~Zn~~ ~Zd~~
	//                       22-^ 19-^16-^    10-^   5-^   0-^
	switch (to) {
	case aarch64::zv_b:
		sloejit_assert(from == aarch64::zv_h);
		break;
	case aarch64::zv_h:
		sloejit_assert(from == aarch64::zv_s);
		break;
	case aarch64::zv_s:
		sloejit_assert(from == aarch64::zv_d);
		break;
	default:
		sloejit_assert(false);
	}
	uint32_t sz = get_zv_sz_bhsd(to);
	uint32_t esize = 8u << sz;
	sloejit_assert(sh <= esize);
	uint32_t h_l_imm3 = 2 * esize - sh;
	uint32_t h = (h_l_imm3 >> 5) & 0b1;
	uint32_t l = (h_l_imm3 >> 3) & 0b11;
	uint32_t imm3 = h_l_imm3 & 0b111;
	bv.push_u32(base | (h << 22) | (l << 19) | (imm3 << 16) | (t << 10) | (rn << 5) | rd);
}

static void create_bin_sqrshrnb_zzi(bytevector &bv, uint32_t rdn, uint32_t rn, uint32_t sh,
                                    aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_sqrshrnbt_zzi<0>(bv, rdn, rn, sh, to, from);
}

static void create_bin_sqrshrnt_zzi(bytevector &bv, uint32_t rdn, uint32_t rn, uint32_t sh,
                                    aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_sqrshrnbt_zzi<1>(bv, rdn, rn, sh, to, from);
}

static void create_bin_srshr_zpi(bytevector &bv, uint32_t rdn, uint32_t pg, uint32_t sh, aarch64::z_type_variant zv) {
	sloejit_assert(rdn < 32);
	sloejit_assert(sh > 0);
	// srshr_z_p_zi.xml
	uint32_t base = 0b00000100'00'001100100'000'00'000'00000u;
	//                         h~           pg~ l~imm3 ~Zdn~
	//                                     10-^    5-^   0-^
	uint32_t sz = get_zv_sz_bhsd(zv);
	uint32_t esize = 8u << sz;
	sloejit_assert(sh <= esize);
	uint32_t h_l_imm3 = 2 * esize - sh;
	uint32_t h = (h_l_imm3 >> 5) & 0b11;
	uint32_t l = (h_l_imm3 >> 3) & 0b11;
	uint32_t imm3 = h_l_imm3 & 0b111;
	bv.push_u32(base | (h << 22) | (pg << 10) | (l << 8) | (imm3 << 5) | rdn);
}

template<uint32_t opa, uint32_t opb, uint32_t t>
static void create_bin_smlasbt_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   aarch64::z_type_variant to, aarch64::z_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// smullb_z_zz.xml
	// smullt_z_zz.xml
	// smlalb_z_zzz.xml
	// smlalt_z_zzz.xml
	// smlslb_z_zzz.xml
	// smlslt_z_zzz.xml
	uint32_t base = 0b0100010'0'00'0'00000'01'00'0'0'00000'00000u;
	//                      opb sz   ~Zm~~   opa   T ~Zn~~ ~Zd~~
	//                     24-^  ^-22 16-^  12-^10-^   5-^   0-^
	switch(to) {
	case aarch64::zv_h:
		sloejit_assert(from == aarch64::zv_b);
		break;
	case aarch64::zv_s:
		sloejit_assert(from == aarch64::zv_h);
		break;
	case aarch64::zv_d:
		sloejit_assert(from == aarch64::zv_s);
		break;
	default: sloejit_assert(false);
	}
	uint32_t sz = get_zv_sz_hsd(to);
	bv.push_u32(base | (opb << 24) | (sz << 22) | (rm << 16) | (opa << 12) | (t << 10) | (rn << 5) | rd);
}

static void create_bin_smlalb_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_smlasbt_zzz<0b00, 0b0, 0b0>(bv, rd, rn, rm, to, from);
}

static void create_bin_smlalt_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_smlasbt_zzz<0b00, 0b0, 0b1>(bv, rd, rn, rm, to, from);
}

static void create_bin_smlslb_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_smlasbt_zzz<0b01, 0b0, 0b0>(bv, rd, rn, rm, to, from);
}

static void create_bin_smlslt_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_smlasbt_zzz<0b01, 0b0, 0b1>(bv, rd, rn, rm, to, from);
}

static void create_bin_smullb_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_smlasbt_zzz<0b11, 0b1, 0b0>(bv, rd, rn, rm, to, from);
}

static void create_bin_smullt_zzz(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                  aarch64::z_type_variant to, aarch64::z_type_variant from) {
	create_bin_smlasbt_zzz<0b11, 0b1, 0b1>(bv, rd, rn, rm, to, from);
}

template <uint32_t op, uint32_t t>
static void create_bin_smlaslbt_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                     unsigned lane, aarch64::z_type_variant to,
                                     aarch64::z_type_variant from) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(rm < 32);
	// smullb_z_zzi.xml
	// smullt_z_zzi.xml
	// smlalb_z_zzzi.xml
	// smlalt_z_zzzi.xml
	// smlslb_z_zzzi.xml
	// smlslt_z_zzzi.xml
	switch (to) {
	case aarch64::zv_s: {
		sloejit_assert(from == aarch64::zv_h);
		sloejit_assert(lane < 8);
		sloejit_assert(rm < 8);
		uint32_t base = 0b01000100'00'1'00'0001'00'0'0'0'00000'00000u;
		//                         sz   h~ Zm~  op   l T ~Zn~~ ~Zd~~
		//                       22-^ 19-^16-^13-^11-^ ^-10  ^-5 0-^
		uint32_t sz = 0b10;
		uint32_t h = (lane >> 1) & 0b11;
		uint32_t l = lane & 0b1;
		bv.push_u32(base | (sz << 22) | (h << 19) | (rm << 16) | (op << 13) | (l << 11) | (t << 10) | (rn << 5) | rd);
		break;
	}
	case aarch64::zv_d: {
		sloejit_assert(from == aarch64::zv_s);
		sloejit_assert(lane < 4);
		sloejit_assert(rm < 16);
		uint32_t base = 0b01000100'00'1'0'00001'00'0'0'0'00000'00000u;
		//                         sz   h  Zm~  op   l T ~Zn~~ ~Zd~~
		//                       22-^20-^ 16-^13-^11-^ ^-10  ^-5 0-^
		uint32_t sz = 0b11;
		uint32_t h = (lane >> 1) & 0b1;
		uint32_t l = lane & 0b1;
		bv.push_u32(base | (sz << 22) | (h << 20) | (rm << 16) | (op << 13) | (l << 11) | (t << 10) | (rn << 5) | rd);
		break;
	}
	default: sloejit_assert(false);
	}
}

static void create_bin_smlalb_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::z_type_variant to,
                                   aarch64::z_type_variant from) {
	create_bin_smlaslbt_zzzl<0b00, 0b0>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smlalt_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::z_type_variant to,
                                   aarch64::z_type_variant from) {
	create_bin_smlaslbt_zzzl<0b00, 0b1>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smlslb_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::z_type_variant to,
                                   aarch64::z_type_variant from) {
	create_bin_smlaslbt_zzzl<0b01, 0b0>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smlslt_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::z_type_variant to,
                                   aarch64::z_type_variant from) {
	create_bin_smlaslbt_zzzl<0b01, 0b1>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smullb_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::z_type_variant to,
                                   aarch64::z_type_variant from) {
	create_bin_smlaslbt_zzzl<0b10, 0b0>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_smullt_zzzl(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t rm,
                                   unsigned lane, aarch64::z_type_variant to,
                                   aarch64::z_type_variant from) {
	create_bin_smlaslbt_zzzl<0b10, 0b1>(bv, rd, rn, rm, lane, to, from);
}

static void create_bin_srshr_qqi(bytevector &bv, uint32_t rd, uint32_t rn, uint32_t sh,
                                 aarch64::q_type_variant qv) {
	sloejit_assert(rd < 32);
	sloejit_assert(rn < 32);
	sloejit_assert(sh > 0);
	// srshr_advsimd.xml
	uint32_t base = 0b0'0'0011110'0000000'001001'00000'00000u;
	//                  Q         ~imm~~~        ~Rn~~ ~Rd~~
	//               30-^            16-^          5-^   0-^
	uint32_t q = 0, imm = 0;
	uint32_t size = 0;
	switch (qv) {
	case aarch64::qv_8b:
		q = 0;
		size = 8;
		break;
	case aarch64::qv_16b:
		q = 1;
		size = 8;
		break;
	case aarch64::qv_4h:
		q = 0;
		size = 16;
		break;
	case aarch64::qv_8h:
		q = 1;
		size = 16;
		break;
	case aarch64::qv_2s:
		q = 0;
		size = 32;
		break;
	case aarch64::qv_4s:
		q = 1;
		size = 32;
		break;
	case aarch64::qv_2d:
		q = 1;
		size = 64;
		break;
	default: sloejit_assert(false);
	}
	sloejit_assert(sh <= size);
	imm = 2 * size - sh;
	bv.push_u32(base | (q << 30) | (imm << 16) | (rn << 5) | rd);
}

static uint32_t normalise_x_allow_xzr(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::xzr.id);
	return r.id == aarch64::xzr.id ? 31 : r.id - aarch64::x0.id;
}

static std::string emit_asm_x_omit_xzr(const instruction &instr, const char *prefix, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::xzr.id);
	if (r.id == aarch64::xzr.id) {
		return "";
	}
	std::ostringstream sstm;
	sstm << prefix << "x" << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static std::string emit_asm_x_allow_xzr(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::xzr.id);
	if (r.id == aarch64::xzr.id) {
		return "xzr";
	}
	std::ostringstream sstm;
	sstm << "x" << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static std::string emit_asm_r_allow_rzr_zv(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(!instr.literals.empty());
	auto zv = static_cast<aarch64::z_type_variant>(instr.literals.back());
	std::string prefix = zv != aarch64::zv_d ? "w" : "x";
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::xzr.id);
	if (r.id == aarch64::xzr.id) {
		return prefix + "zr";
	}
	std::ostringstream sstm;
	sstm << prefix << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static std::string emit_asm_r_allow_rzr_qv(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(!instr.literals.empty());
	auto qv = static_cast<aarch64::q_type_variant>(instr.literals.back());
	std::string prefix = qv == aarch64::qv_2d ? "x" : "w";
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::xzr.id);
	if (r.id == aarch64::xzr.id) {
		return prefix + "zr";
	}
	std::ostringstream sstm;
	sstm << prefix << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_x_allow_sp(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::sp.id);
	return r.id == aarch64::sp.id ? 31 : r.id - aarch64::x0.id;
}

static std::string emit_asm_x_allow_sp(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::sp.id);
	if (r.id == aarch64::sp.id) {
		return "sp";
	}
	std::ostringstream sstm;
	sstm << "x" << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_b(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::b0.id && r.id <= aarch64::b31.id);
	return r.id - aarch64::b0.id;
}

static std::string emit_asm_b(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::b0.id && r.id <= aarch64::b31.id);
	std::ostringstream sstm;
	sstm << "b" << (r.id - aarch64::b0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_h(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::h0.id && r.id <= aarch64::h31.id);
	return r.id - aarch64::h0.id;
}

static std::string emit_asm_h(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::h0.id && r.id <= aarch64::h31.id);
	std::ostringstream sstm;
	sstm << "h" << (r.id - aarch64::h0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_s(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::s0.id && r.id <= aarch64::s31.id);
	return r.id - aarch64::s0.id;
}

static std::string emit_asm_s(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::s0.id && r.id <= aarch64::s31.id);
	std::ostringstream sstm;
	sstm << "s" << (r.id - aarch64::s0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_d(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::d0.id && r.id <= aarch64::d31.id);
	return r.id - aarch64::d0.id;
}

static std::string emit_asm_d(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::d0.id && r.id <= aarch64::d31.id);
	std::ostringstream sstm;
	sstm << "d" << (r.id - aarch64::d0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_q(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::q0.id && r.id <= aarch64::q31.id);
	return r.id - aarch64::q0.id;
}

static std::string emit_asm_q(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::q0.id && r.id <= aarch64::q31.id);
	std::ostringstream sstm;
	sstm << "q" << (r.id - aarch64::q0.id);
	return std::move(sstm).str();
}

static std::string emit_asm_v(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::q0.id && r.id <= aarch64::q31.id);
	std::ostringstream sstm;
	sstm << "v" << (r.id - aarch64::q0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_z(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::z0.id && r.id <= aarch64::z31.id);
	return r.id - aarch64::z0.id;
}

static uint32_t normalise_za(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id == aarch64::za.id);
	return 0;
}

static uint32_t normalise_x_12_15(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::x12.id && r.id <= aarch64::x15.id);
	return r.id - aarch64::x0.id;
}

static std::string emit_asm_z(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::z0.id && r.id <= aarch64::z31.id);
	std::ostringstream sstm;
	sstm << "z" << (r.id - aarch64::z0.id);
	return std::move(sstm).str();
}

static std::string emit_asm_za(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id == aarch64::za.id);
	return "za";
}

static std::string emit_asm_smopt(const instruction &instr, uint32_t i) {
	auto r = instr.get_literal(i);
	switch(r){
	case aarch64::smopt_sm: return "sm";
	case aarch64::smopt_za: return "za";
	case aarch64::smopt_both: return "";
	default: __builtin_unreachable();
	}
}

static std::string get_ptrue_pat_str(const instruction &instr, uint32_t i) {
	auto pat = static_cast<aarch64::ptrue_pat>(instr.get_literal(i));
	switch(pat){
	case aarch64::ptrue_pat_pow2: return "pow2";
	case aarch64::ptrue_pat_vl1: return "vl1";
	case aarch64::ptrue_pat_vl2: return "vl2";
	case aarch64::ptrue_pat_vl3: return "vl3";
	case aarch64::ptrue_pat_vl4: return "vl4";
	case aarch64::ptrue_pat_vl5: return "vl5";
	case aarch64::ptrue_pat_vl6: return "vl6";
	case aarch64::ptrue_pat_vl7: return "vl7";
	case aarch64::ptrue_pat_vl8: return "vl8";
	case aarch64::ptrue_pat_vl16: return "vl16";
	case aarch64::ptrue_pat_vl32: return "vl32";
	case aarch64::ptrue_pat_vl64: return "vl64";
	case aarch64::ptrue_pat_vl128: return "vl128";
	case aarch64::ptrue_pat_vl256: return "vl256";
	case aarch64::ptrue_pat_mul4: return "mul4";
	case aarch64::ptrue_pat_mul3: return "mul3";
	case aarch64::ptrue_pat_all: return "all";
	default: __builtin_unreachable();
	}
}

static std::string emit_asm_hv(const instruction &instr, uint32_t i) {
	auto r = instr.get_literal(i);
	switch(r){
	case aarch64::hvopt_h: return "h";
	case aarch64::hvopt_v: return "v";
	default: __builtin_unreachable();
	}
}

static std::string emit_asm_w(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::x0.id && r.id <= aarch64::x30.id);
	std::ostringstream sstm;
	sstm << "w" << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static std::string emit_asm_w_allow_wzr(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert((r.id >= aarch64::x0.id && r.id <= aarch64::x30.id) || r.id == aarch64::xzr.id);
	if (r.id == aarch64::xzr.id) {
		return "wzr";
	}
	std::ostringstream sstm;
	sstm << "w" << (r.id - aarch64::x0.id);
	return std::move(sstm).str();
}

static uint32_t normalise_p_low8(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::p0.id && r.id <= aarch64::p7.id);
	return r.id - aarch64::p0.id;
}

static uint32_t normalise_p_all16(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::p0.id && r.id <= aarch64::p15.id);
	return r.id - aarch64::p0.id;
}

static std::string_view get_qv_long_str(const instruction &instr, int idx) {
	int qv = instr.get_literal(idx);
	switch ((aarch64::q_type_variant)qv) {
	case aarch64::qv_8b: return "8b";
	case aarch64::qv_16b: return "16b";
	case aarch64::qv_4h: return "4h";
	case aarch64::qv_8h: return "8h";
	case aarch64::qv_2s: return "2s";
	case aarch64::qv_4s: return "4s";
	case aarch64::qv_1d: return "1d";
	case aarch64::qv_2d: return "2d";
	default: sloejit_assert(false);
	}
	return "";
}

static std::string_view get_qv_short_str(const instruction &instr, int idx) {
	int qv = instr.get_literal(idx);
	switch ((aarch64::q_type_variant)qv) {
	case aarch64::qv_8b: return "b";
	case aarch64::qv_16b: return "b";
	case aarch64::qv_4h: return "h";
	case aarch64::qv_8h: return "h";
	case aarch64::qv_2s: return "s";
	case aarch64::qv_4s: return "s";
	case aarch64::qv_1d: return "d";
	case aarch64::qv_2d: return "d";
	default: sloejit_assert(false);
	}
	return "";
}

static std::string_view get_zv_str(const instruction &instr, int idx) {
	int zv = instr.get_literal(idx);
	switch ((aarch64::z_type_variant)zv) {
	case aarch64::zv_b: return "b";
	case aarch64::zv_h: return "h";
	case aarch64::zv_s: return "s";
	case aarch64::zv_d: return "d";
	case aarch64::zv_q: return "q";
	default: sloejit_assert(false);
	}
	return "";
}

static std::string emit_asm_p(const instruction &instr, uint32_t i) {
	auto r = instr.get_reg(i);
	sloejit_assert(r.id >= aarch64::p0.id && r.id <= aarch64::p15.id);
	std::ostringstream sstm;
	sstm << "p" << (r.id - aarch64::p0.id);
	return std::move(sstm).str();
}

static int64_t get_literal(const instruction &instr, uint32_t i) {
	return instr.get_literal(i);
}

static int64_t get_literal_else_reloc(const instruction &i, int64_t pc, std::vector<reloc_info> *relocs,
                                      int reloc_type) {
	if (!i.literals.empty()) {
		return i.get_literal(0);
	}
	sloejit_assert(relocs);
	auto *target = i.targets.at(0);
	// if we're delaying emitting this because we need a relocation,
	// put zeros as a placeholder to be filled in by the linker.
	relocs->emplace_back(target->name, pc, reloc_type, 0);
	return 0;
}

static std::string emit_asm_integer(const instruction &instr, uint32_t i) {
	int64_t x = instr.get_literal(i);
	return std::to_string(x);
}

static std::string emit_asm_literal_omit0(const instruction &instr, const char *prefix, uint32_t i) {
	int64_t x = instr.get_literal(i);
	if (x == 0) {
		return "";
	}
	std::ostringstream sstm;
	sstm << prefix << "#" << x;
	return std::move(sstm).str();
}

static std::string emit_asm_literal(const instruction &instr, uint32_t i) {
	int64_t x = instr.get_literal(i);
	std::ostringstream sstm;
	sstm << "#" << x;
	return std::move(sstm).str();
}

static std::string emit_asm_movx_literal(const instruction &instr, uint32_t i) {
	uint64_t imm = static_cast<uint64_t>(instr.get_literal(i));
	uint32_t shift = (imm & 0x0000'0000'0000'fffful) == imm
	                     ? 0
	                     : (imm & 0x0000'0000'ffff'0000ul) == imm
	                           ? 1
	                           : (imm & 0x0000'ffff'0000'0000ul) == imm
	                                 ? 2
	                                 : (imm & 0xffff'0000'0000'0000ul) == imm ? 3 : ~0u;
	sloejit_assert(shift != ~0u);
	imm = (imm >> (shift * 16ul)) & 0xffffu;
	std::ostringstream sstm;
	sstm << "#" << imm;
	if (shift != 0) {
		sstm << ", lsl #" << (shift * 16u);
	}
	return std::move(sstm).str();
}

static std::string_view emit_asm_condition_code(const instruction &instr, uint32_t i) {
	int cond = instr.get_literal(i);
	sloejit_assert(0x0 <= cond && cond < 0xF);

	switch (cond) {
	case 0x0: return "eq";
	case 0x1: return "ne";
	case 0x2: return "hs";
	case 0x3: return "lo";
	case 0x4: return "mi";
	case 0x5: return "pl";
	case 0x6: return "vs";
	case 0x7: return "vc";
	case 0x8: return "hi";
	case 0x9: return "ls";
	case 0xA: return "ge";
	case 0xB: return "lt";
	case 0xC: return "gt";
	case 0xD: return "le";
	case 0xE: return "al";
	default: sloejit_assert(false && "unreachable");
	}
	return "";
}

static std::string emit_asm_lsl_omit_lsl0(const instruction &instr, const char *prefix, uint32_t i) {
	int x = instr.get_literal(i);
	if (x == 0) {
		return "";
	}
	std::ostringstream sstm;
	sstm << prefix << "lsl #" << x;
	return std::move(sstm).str();
}

static std::string emit_asm_lsl(const instruction &instr, uint32_t i) {
	int x = instr.get_literal(i);
	std::ostringstream sstm;
	sstm << "lsl #" << x;
	return std::move(sstm).str();
}

static std::string emit_asm_sxtw_or_lsl_zv(const instruction &instr, uint32_t i) {
	int sh = instr.get_literal(0);
	auto zv = static_cast<aarch64::z_type_variant>(instr.get_literal(1));
	if (zv == aarch64::zv_s) {
		std::ostringstream sstm;
		sstm << "sxtw #" << sh;
		return std::move(sstm).str();
	}
	return emit_asm_lsl(instr, i);
}

static std::string emit_asm_target_label(const instruction &instr, uint32_t i) {
	if (instr.targets.empty()) {
		return emit_asm_literal(instr, i);
	}
	std::ostringstream sstm;
	sstm << instr.targets.at(0)->name;
	return std::move(sstm).str();
}

static std::string emit_asm_mul_vl_omit0(const instruction & instr, std::string prefix, uint32_t i) {
	int imm = instr.get_literal(i);
	if (imm == 0) {
		return "";
	}
	std::ostringstream sstm;
	sstm << prefix << emit_asm_literal(instr, i) << ", mul vl";
	return std::move(sstm).str();
}

static std::string emit_lo12(const instruction &instr, uint32_t i) {
	if (instr.targets.empty()) {
		return emit_asm_literal(instr, i);
	}
	std::ostringstream sstm;
	sstm << "PAGEOFF(" << emit_asm_target_label(instr, i) << ")";
	return std::move(sstm).str();
}

static std::string emit_hi21(const instruction &instr, uint32_t i) {
	if (instr.targets.empty()) {
		return emit_asm_literal(instr, i);
	}
	std::ostringstream sstm;
	sstm << "PAGE(" << emit_asm_target_label(instr, i) << ")";
	return std::move(sstm).str();
}

%for instr in opcodes.values():
<% i = " i" if instr.operands else "" %>\
<% pc = " pc" if any(op.parent.is_target for op in instr.operands) else "" %>\
<% relocs = "relocs" if any(op.parent.is_target for op in instr.operands) else "" %>\
static void emit_bin_${instr.name}(bytevector &bv, const instruction &${i}, int64_t${pc}, std::vector<reloc_info> *${relocs}) {
<% args = ["bv"] %>\
<% reg_num = 0 %>\
<% imm_num = 0 %>\
%for op in instr.operands:
%if op.parent.is_target:
<% assert imm_num == 0 %>\
	auto imm0 = ${op.parent.normalize};
<% args.append("imm{}".format(imm_num)) %>\
<% imm_num += 1 %>\
%elif op.parent.is_immediate:
	auto imm${imm_num} = ${op.parent.normalize}(i, ${imm_num});
<% args.append("imm{}".format(imm_num)) %>\
<% imm_num += 1 %>\
%else:
	auto reg${reg_num} = ${op.parent.normalize}(i, ${reg_num});
<% args.append("reg{}".format(reg_num)) %>\
<% reg_num += 1 %>\
%endif
%endfor
	create_bin_${instr.name}(${', '.join(args)});
}
%endfor

%for instr in opcodes.values():
%for name_suffix, emit_asm in instr.emit_asms.items():
<% name_suffix = f"_{name_suffix}" if name_suffix != "" else "" %>\
static std::string emit_asm_${instr.name}${name_suffix}(const instruction &i) {
	(void)i;
	std::ostringstream sstm;
<% assert emit_asm is not None %>\
%for op in emit_asm:
%if type(op) is str:
	sstm << "${op}";
%elif op.parent.is_immediate and op.emit_asm is None:
	sstm << emit_asm_literal(i, ${op.imm_idx});
%else:
<% assert op.emit_asm is not None %>\
<% idx = op.imm_idx if op.parent.is_immediate else op.reg_idx %>\
%if op.emit_asm.prefix is not None:
	sstm << ${op.emit_asm.fn_name}(i, "${op.emit_asm.prefix}", ${idx});
%else:
	sstm << ${op.emit_asm.fn_name}(i, ${idx});
%endif
%endif  # type(op) / op.parent.is_immediate ...
%endfor # op in emit_asm
	return std::move(sstm).str();
}
%endfor  # emit_asm
%endfor  # instr

template<typename EmitAsm>
std::string dispatch_emit_asm(const instruction &i, bool cond,
			      EmitAsm emit_if_true, EmitAsm emit_otherwise) {
	return cond ? emit_if_true(i) : emit_otherwise(i);
}

static std::string emit_asm_orr_rrr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(1).id == aarch64::xzr.id,
				 emit_asm_orr_rrr_as_mov,
				 emit_asm_orr_rrr_as_orr);
}

static std::string emit_asm_orr_qqq(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(1).id == i.get_reg(2).id,
				 emit_asm_orr_qqq_as_mov,
				 emit_asm_orr_qqq_as_orr);
}

static std::string emit_asm_orr_zzz(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(1).id == i.get_reg(2).id,
				 emit_asm_orr_zzz_as_mov,
				 emit_asm_orr_zzz_as_orr);
}

static std::string emit_asm_subs_rri(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(0).id == aarch64::xzr.id,
				 emit_asm_subs_rri_as_cmp,
				 emit_asm_subs_rri_as_subs);
}

static std::string emit_asm_subs_rrr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(0).id == aarch64::xzr.id,
				 emit_asm_subs_rrr_as_cmp,
				 emit_asm_subs_rrr_as_subs);
}

static std::string emit_asm_smstart_i(const instruction &i) {
	return dispatch_emit_asm(i, i.get_literal(0) == aarch64::smopt_both,
				 emit_asm_smstart_i_no_suffix,
				 emit_asm_smstart_i_with_suffix);
}

static std::string emit_asm_smstop_i(const instruction &i) {
	return dispatch_emit_asm(i, i.get_literal(0) == aarch64::smopt_both,
				 emit_asm_smstop_i_no_suffix,
				 emit_asm_smstop_i_with_suffix);
}

static std::string emit_asm_ptrue_p(const instruction &i) {
	auto pat = static_cast<aarch64::ptrue_pat>(i.get_literal(0));
	return dispatch_emit_asm(i, pat == aarch64::ptrue_pat_all,
				 emit_asm_ptrue_p_no_pattern,
				 emit_asm_ptrue_p_with_pattern);
}

static std::string emit_asm_ld1b_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_ld1b_alorlprr_no_offset,
				 emit_asm_ld1b_alorlprr_with_offset);
}

static std::string emit_asm_ld1h_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_ld1h_alorlprr_no_offset,
				 emit_asm_ld1h_alorlprr_with_offset);
}

static std::string emit_asm_ld1w_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_ld1w_alorlprr_no_offset,
				 emit_asm_ld1w_alorlprr_with_offset);
}

static std::string emit_asm_ld1d_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_ld1d_alorlprr_no_offset,
				 emit_asm_ld1d_alorlprr_with_offset);
}

static std::string emit_asm_ld1q_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_ld1q_alorlprr_no_offset,
				 emit_asm_ld1q_alorlprr_with_offset);
}

static std::string emit_asm_st1b_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_st1b_alorlprr_no_offset,
				 emit_asm_st1b_alorlprr_with_offset);
}

static std::string emit_asm_st1h_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_st1h_alorlprr_no_offset,
				 emit_asm_st1h_alorlprr_with_offset);
}

static std::string emit_asm_st1w_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_st1w_alorlprr_no_offset,
				 emit_asm_st1w_alorlprr_with_offset);
}

static std::string emit_asm_st1d_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_st1d_alorlprr_no_offset,
				 emit_asm_st1d_alorlprr_with_offset);
}

static std::string emit_asm_st1q_alorlprr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(4).id == aarch64::xzr.id,
				 emit_asm_st1q_alorlprr_no_offset,
				 emit_asm_st1q_alorlprr_with_offset);
}

static std::string emit_asm_madd_rrrr(const instruction &i) {
	return dispatch_emit_asm(i, i.get_reg(3).id == aarch64::xzr.id,
				 emit_asm_madd_rrrr_as_mul,
				 emit_asm_madd_rrrr_as_madd);
}

static regset get_pcs_clobbered() {
	// Some registers do not exist in the Arm64EC ABI. We therefore
	// exclude them from the regset. In particular:
	//
	//    "We can also see how the registers x13, x14, x23, x24, x28, v16-v31
	//     have no representation and, thus, cannot be used in Arm64EC."
	//
	// See https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
	// The restriction on using vector registers has been relaxed in later
	// versions of Arm64EC so we allow their use in these kernels

	using namespace aarch64;
	static regset ret = {
		x0,          x1,          x2,          x3,         x4,         x5,          x6,          x7,
#if !defined(PLFFT_ENABLE_ARM64EC)
		x8,          x9,          x10,         x11,        x12,        x13,         x14,         x15,
#else
		x8,          x9,          x10,         x11,        x12,                                  x15,
#endif
	// x18 may not be used on mac, android or windows. Remove it for all builds to maintain kernel generality
		x16,         x17,                      z0,         z1,         z2,          z3,          z4,
		z5,          z6,          z7,          z8_no_dreg, z9_no_dreg, z10_no_dreg, z11_no_dreg, z12_no_dreg,
		z13_no_dreg, z14_no_dreg, z15_no_dreg, z16,        z17,        z18,         z19,         z20,
		z21,         z22,         z23,         z24,        z25,        z26,         z27,         z28,
		z29,         z30,         z31,         p0,         p1,         p2,          p3,          p4,
		p5,          p6,          p7,          p8,         p9,         p10,         p11,         p12,
		p13,         p14,         p15,
	};
	return ret;
}

static regset get_sm_clobbered() {
	// On entry to streaming mode, all vector registers must be preserved
	using namespace aarch64;
	static regset ret = {
	   z0,  z1,  z2,  z3,  z4,  z5,  z6,  z7,
	   z8,  z9, z10, z11, z12, z13, z14, z15,
	  z16, z17, z18, z19, z20, z21, z22, z23,
	  z24, z25, z26, z27, z28, z29, z30, z31
	};
	return ret;
}

%for instr in opcodes.values():
<% input_mask = [io != "=" for io in instr.inout] %>\
<% output_mask = [io in ["=", "+"] for io in instr.inout] %>\
<% input_active_mask = [["0", "aarch64::{}_regs".format(reg.narrow_name)][is_input] for is_input, reg in zip(input_mask, instr.regs)] %>\
<% output_active_mask = [["0", "aarch64::{}_regs".format(reg.wide_name)][is_output] for is_output, reg in zip(output_mask, instr.regs)] %>\
<% clobbers = instr.clobbers if instr.clobbers is not None else "{}" %>\
static instr_base ${instr.name}_base {
	(int) aarch64::opcode::${instr.name},
	sloejit::${instr.kind}, // kind
	{ ${', '.join([str(x).lower() for x in input_mask])} }, // input mask
	{ ${', '.join([str(x).lower() for x in output_mask])} }, // output mask
	{ ${', '.join(input_active_mask)} }, // input active mask
	{ ${', '.join(output_active_mask)} }, // output active mask
	${clobbers}, // additional clobbers
	&emit_bin_${instr.name},
	&emit_asm_${instr.name},
};
%endfor

void aarch64::instr_builder::make_x_madd_rrrr(reg rd, reg rn, reg rm, reg ra) {
	reg_assert_classes_equal_to(x_regs, rd, rn, rm, ra);
	std::vector<reg> regs{ rd, rn, rm, ra };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &madd_rrrr_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_x_madd_rrr(reg rn, reg rm, reg ra) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm, ra);
	reg rd = b->fresh_vreg(x_space, rc);
	make_x_madd_rrrr(rd, rn, rm, ra);
	return rd;
}

void aarch64::instr_builder::make_x_mul_rrr(reg rd, reg rn, reg rm) {
	make_x_madd_rrrr(rd, rn, rm, { xzr });
}

reg aarch64::instr_builder::make_x_mul_rr(reg rn, reg rm) {
	return make_x_madd_rrr(rn, rm, { xzr });
}

static void make_x_twoarg_rrr(block *b, instruction *instr_pos, reg rd, reg rn, reg rm,
                              const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_x_twoarg_rr(block *b, instruction *instr_pos, reg rn, reg rm, const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::x_space, rc);
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, base);
	return rd;
}

static void make_x_twoarg_rri(block *b, instruction *instr_pos, reg rd, reg rn, uint32_t imm,
                              const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_x_twoarg_ri(block *b, instruction *instr_pos, reg rn, uint32_t imm, const instr_base *base) {
	auto rc = reg_get_active_mask(rn);
	reg rd = b->fresh_vreg(aarch64::x_space, rc);
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, base);
	return rd;
}

static void make_rrr_shift(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::shift_amount sh,
                           const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ sh };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_rr_shift(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::shift_amount sh,
                         const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::x_space, rc);
	make_rrr_shift(b, instr_pos, rd, rn, rm, sh, base);
	return rd;
}

void aarch64::instr_builder::make_x_sub_rrr(reg rd, reg rn, reg rm, shift_amount sh) {
	make_rrr_shift(b, instr_pos, rd, rn, rm, sh, &sub_rrr_base);
}

reg aarch64::instr_builder::make_x_sub_rr(reg rn, reg rm, shift_amount sh) {
	return make_rr_shift(b, instr_pos, rn, rm, sh, &sub_rrr_base);
}

void aarch64::instr_builder::make_x_sub_rri(reg rd, reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &sub_rri_base);
}

reg aarch64::instr_builder::make_x_sub_ri(reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &sub_rri_base);
}

void aarch64::instr_builder::make_x_subs_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &subs_rrr_base);
}

reg aarch64::instr_builder::make_x_subs_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &subs_rrr_base);
}

void aarch64::instr_builder::make_x_subs_rri(reg rd, reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &subs_rri_base);
}

reg aarch64::instr_builder::make_x_subs_ri(reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &subs_rri_base);
}

void aarch64::instr_builder::make_x_add_rrr(reg rd, reg rn, reg rm, shift_amount sh) {
	make_rrr_shift(b, instr_pos, rd, rn, rm, sh, &add_rrr_base);
}

reg aarch64::instr_builder::make_x_add_rr(reg rn, reg rm, shift_amount sh) {
	return make_rr_shift(b, instr_pos, rn, rm, sh, &add_rrr_base);
}

void aarch64::instr_builder::make_x_add_rri(reg rd, reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &add_rri_base);
}

reg aarch64::instr_builder::make_x_add_ri(reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &add_rri_base);
}

void aarch64::instr_builder::make_x_add_rrb(reg rd, reg rn, branch_target *bt) {
	reg_assert_classes_equal_to(x_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, &add_rri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_x_add_rb(reg rn, branch_target *bt) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_x_add_rrb(rd, rn, bt);
	return rd;
}

void aarch64::instr_builder::make_x_adds_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &adds_rrr_base);
}

reg aarch64::instr_builder::make_x_adds_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &adds_rrr_base);
}

void aarch64::instr_builder::make_x_adds_rri(reg rd, reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &adds_rri_base);
}

reg aarch64::instr_builder::make_x_adds_ri(reg rn, uint32_t imm) {
	sloejit_assert((imm & 0xfffu) == imm || (imm & 0xfff000u) == imm);
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &adds_rri_base);
}

void aarch64::instr_builder::make_x_sdiv_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &sdiv_rrr_base);
}

reg aarch64::instr_builder::make_x_sdiv_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &sdiv_rrr_base);
}

void aarch64::instr_builder::make_x_udiv_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &udiv_rrr_base);
}

reg aarch64::instr_builder::make_x_udiv_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &udiv_rrr_base);
}

void aarch64::instr_builder::make_lsl_rri(reg rd, reg rn, uint32_t imm) {
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &lsl_rri_base);
}

reg aarch64::instr_builder::make_lsl_ri(reg rn, uint32_t imm) {
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &lsl_rri_base);
}

void aarch64::instr_builder::make_lsr_rri(reg rd, reg rn, uint32_t imm) {
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &lsr_rri_base);
}

reg aarch64::instr_builder::make_lsr_ri(reg rn, uint32_t imm) {
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &lsr_rri_base);
}

void aarch64::instr_builder::make_asr_rri(reg rd, reg rn, uint32_t imm) {
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &asr_rri_base);
}

reg aarch64::instr_builder::make_asr_ri(reg rn, uint32_t imm) {
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &asr_rri_base);
}

reg aarch64::instr_builder::make_fadd_hh(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fadd_hhh(rd, rn, rm);
	return rd;
}

reg aarch64::instr_builder::make_fadd_ss(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fadd_sss(rd, rn, rm);
	return rd;
}

reg aarch64::instr_builder::make_fadd_dd(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fadd_ddd(rd, rn, rm);
	return rd;
}

void aarch64::instr_builder::make_fadd_hhh(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fadd_hhh_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fadd_sss(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fadd_sss_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fadd_ddd(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fadd_ddd_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fsub_hh(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fsub_hhh(rd, rn, rm);
	return rd;
}

reg aarch64::instr_builder::make_fsub_ss(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fsub_sss(rd, rn, rm);
	return rd;
}

reg aarch64::instr_builder::make_fsub_dd(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fsub_ddd(rd, rn, rm);
	return rd;
}

void aarch64::instr_builder::make_fsub_hhh(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fsub_hhh_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fsub_sss(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fsub_sss_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fsub_ddd(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fsub_ddd_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fmul_hh(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fmul_hhh(rd, rn, rm);
	return rd;
}

reg aarch64::instr_builder::make_fmul_ss(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fmul_sss(rd, rn, rm);
	return rd;
}

reg aarch64::instr_builder::make_fmul_dd(reg rn, reg rm) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_fmul_ddd(rd, rn, rm);
	return rd;
}

void aarch64::instr_builder::make_fmul_hhh(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fmul_hhh_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fmul_sss(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fmul_sss_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fmul_ddd(reg rd, reg rn, reg rm) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fmul_ddd_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static uint8_t get_active_mask_from_qv(aarch64::q_type_variant qv) {
	switch (qv) {
	case aarch64::qv_8b: return aarch64::d_regs;
	case aarch64::qv_16b: return aarch64::q_regs;
	case aarch64::qv_4h: return aarch64::d_regs;
	case aarch64::qv_8h: return aarch64::q_regs;
	case aarch64::qv_2s: return aarch64::d_regs;
	case aarch64::qv_4s: return aarch64::q_regs;
	case aarch64::qv_1d: return aarch64::d_regs;
	case aarch64::qv_2d: return aarch64::q_regs;
	}
	sloejit_assert(false);
	return 0;
}

reg aarch64::instr_builder::make_fadd_qq(reg rn, reg rm, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, get_active_mask_from_qv(qv));
	make_fadd_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_fadd_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fadd_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fsub_qq(reg rn, reg rm, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, get_active_mask_from_qv(qv));
	make_fsub_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_fsub_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fsub_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fmul_qq(reg rn, reg rm, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, get_active_mask_from_qv(qv));
	make_fmul_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_fmul_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fmul_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fmul_qql(reg rn, reg rm, int lane, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, get_active_mask_from_qv(qv));
	make_fmul_qqql(rd, rn, rm, lane, qv);
	return rd;
}

static const regset_one_space &aarch64_regs_for_space(uint64_t space_id);
static const regset_one_space &aarch64_regs_for_space_low_half(uint64_t space_id);
static const regset_one_space &aarch64_regs_for_space_low_quarter(uint64_t space_id);

void aarch64::instr_builder::make_fmul_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ lane, qv };
	std::vector<branch_target *> targets;
	if (qv == aarch64::q_type_variant::qv_4h || qv == aarch64::q_type_variant::qv_8h) {
		// The indexed fmul requires the second input register to be restricted to
		// the range v0 - v15 for half precision
		std::vector<bool> input_mask;
		std::vector<bool> output_mask;
		std::vector<uint8_t> input_active_mask;
		std::vector<uint8_t> output_active_mask;
		auto all_vregs = aarch64_regs_for_space(aarch64::v_space).as_regset();
		auto low_vregs = aarch64_regs_for_space_low_half(aarch64::v_space).as_regset();
		std::vector<regset> reg_choices = { all_vregs, all_vregs, low_vregs };
		make_instr(*b, instr_pos, &fmul_qqql_base, tag, std::move(regs), std::move(input_mask), std::move(output_mask), std::move(input_active_mask), std::move(output_active_mask),
		    std::move(reg_choices), std::move(literals), std::move(targets));
	}
	else {
		make_instr(*b, instr_pos, &fmul_qqql_base, tag, std::move(regs), std::move(literals), std::move(targets));
	}
}

static void make_threearg_zzz(block *b, instruction *instr_pos, reg rd, reg rn, reg rm,
                              aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_threearg_zz(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::z_type_variant zv,
                            const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, base);
	return rd;
}

static void make_threearg_zpz(block *b, instruction *instr_pos, reg rdn, reg pg, reg rm,
                              aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rdn, pg, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_add_zi(reg rdn, unsigned imm, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn);
	std::vector<reg> regs{ rdn };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &add_zi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sub_zi(reg rdn, unsigned imm, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn);
	std::vector<reg> regs{ rdn };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sub_zi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_add_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &add_zzz_base);
}

void aarch64::instr_builder::make_add_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &add_zzz_base);
}

reg aarch64::instr_builder::make_add_qq(reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_add_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_add_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &add_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sub_qq(reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sub_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_sub_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sub_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sub_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &sub_zzz_base);
}

void aarch64::instr_builder::make_sub_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &sub_zzz_base);
}

void aarch64::instr_builder::make_add_zpz(reg rdn, reg pg, reg rm, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rdn, pg, rm, zv, &add_zpz_base);
}

void aarch64::instr_builder::make_sub_zpz(reg rdn, reg pg, reg rm, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rdn, pg, rm, zv, &sub_zpz_base);
}

void aarch64::instr_builder::make_mul_zpz(reg rdn, reg pg, reg rm, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rdn, pg, rm, zv, &mul_zpz_base);
}

reg aarch64::instr_builder::make_fadd_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &fadd_zzz_base);
}

void aarch64::instr_builder::make_fadd_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &fadd_zzz_base);
}

reg aarch64::instr_builder::make_fsub_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &fsub_zzz_base);
}

void aarch64::instr_builder::make_fsub_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &fsub_zzz_base);
}

reg aarch64::instr_builder::make_fmul_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &fmul_zzz_base);
}

void aarch64::instr_builder::make_fmul_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &fmul_zzz_base);
}

void aarch64::instr_builder::make_fadd_zpz(reg rdn, reg pg, reg rm, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rdn, pg, rm, zv, &fadd_zpz_base);
}

void aarch64::instr_builder::make_fsub_zpz(reg rdn, reg pg, reg rm, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rdn, pg, rm, zv, &fsub_zpz_base);
}

void aarch64::instr_builder::make_fmul_zpz(reg rdn, reg pg, reg rm, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rdn, pg, rm, zv, &fmul_zpz_base);
}

reg aarch64::instr_builder::make_fneg_h(reg rn) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_fneg_hh(rd, rn);
	return rd;
}

reg aarch64::instr_builder::make_fneg_s(reg rn) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_fneg_ss(rd, rn);
	return rd;
}

reg aarch64::instr_builder::make_fneg_d(reg rn) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_fneg_dd(rd, rn);
	return rd;
}

void aarch64::instr_builder::make_fneg_hh(reg rd, reg rn) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fneg_hh_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fneg_ss(reg rd, reg rn) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fneg_ss_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fneg_dd(reg rd, reg rn) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fneg_dd_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fneg_q(reg rn, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_fneg_qq(rd, rn, qv);
	return rd;
}

void aarch64::instr_builder::make_fneg_qq(reg rd, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fneg_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fneg_zpz(reg rd, reg pg, reg rn, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rd, pg, rn };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fneg_zpz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_f_acc_op_qqq(block *b, instruction *instr_pos, reg rd, reg rn, reg rm,
                              aarch64::q_type_variant qv, const instr_base *base) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static void make_f_acc_op_zpzz(block *b, instruction *instr_pos, reg rda, reg pg, reg rn, reg rm,
                               aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rda, rn, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rda, pg, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static void make_f_acc_op_qqql(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, int lane,
                               aarch64::q_type_variant qv, const instr_base *base) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ lane, qv };
	std::vector<branch_target *> targets;
	if (qv == aarch64::q_type_variant::qv_4h || qv == aarch64::q_type_variant::qv_8h) {
		// Indexed accumulation requires that the second input register be restricted
		// to the range v0 - v15 for half precision
		std::vector<bool> input_mask;
		std::vector<bool> output_mask;
		std::vector<uint8_t> input_active_mask;
		std::vector<uint8_t> output_active_mask;
		auto all_vregs = aarch64_regs_for_space(aarch64::v_space).as_regset();
		auto low_vregs = aarch64_regs_for_space_low_half(aarch64::v_space).as_regset();
		std::vector<regset> reg_choices = { all_vregs, all_vregs, low_vregs };
		make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(input_mask), std::move(output_mask), std::move(input_active_mask), std::move(output_active_mask),
		    std::move(reg_choices), std::move(literals), std::move(targets));
	}
	else {
		make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
	}
}

static void make_f_acc_op_zzzl(block *b, instruction *instr_pos, reg rda, reg rn, reg rm, int lane,
                               aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rda, rn, rm);
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ lane, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fmla_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	return make_f_acc_op_qqq(b, instr_pos, rd, rn, rm, qv, &fmla_qqq_base);
}

void aarch64::instr_builder::make_fmla_zpzz(reg rda, reg pg, reg rn, reg rm, z_type_variant zv) {
	return make_f_acc_op_zpzz(b, instr_pos, rda, pg, rn, rm, zv, &fmla_zpzz_base);
}

void aarch64::instr_builder::make_fmls_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	return make_f_acc_op_qqq(b, instr_pos, rd, rn, rm, qv, &fmls_qqq_base);
}

void aarch64::instr_builder::make_fmls_zpzz(reg rda, reg pg, reg rn, reg rm, z_type_variant zv) {
	return make_f_acc_op_zpzz(b, instr_pos, rda, pg, rn, rm, zv, &fmls_zpzz_base);
}

void aarch64::instr_builder::make_fmla_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant qv) {
	return make_f_acc_op_qqql(b, instr_pos, rd, rn, rm, lane, qv, &fmla_qqql_base);
}

void aarch64::instr_builder::make_fmla_zzzl(reg rda, reg rn, reg rm, int lane, z_type_variant zv) {
	return make_f_acc_op_zzzl(b, instr_pos, rda, rn, rm, lane, zv, &fmla_zzzl_base);
}

void aarch64::instr_builder::make_fmls_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant qv) {
	return make_f_acc_op_qqql(b, instr_pos, rd, rn, rm, lane, qv, &fmls_qqql_base);
}

void aarch64::instr_builder::make_fmls_zzzl(reg rda, reg rn, reg rm, int lane, z_type_variant zv) {
	return make_f_acc_op_zzzl(b, instr_pos, rda, rn, rm, lane, zv, &fmls_zzzl_base);
}

void aarch64::instr_builder::make_fcmla_qqqi(reg rd, reg rn, reg rm, int rot, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ rot, qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fcmla_qqqi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_fcmla_zpzzi(reg rda, reg pg, reg rn, reg rm, int rot, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rda, rn, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rda, pg, rn, rm };
	std::vector<int64_t> literals{ rot, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fcmla_zpzzi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_cntinc_r(block *b, instruction *instr_pos, reg rd, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_cntinc(block *b, instruction *instr_pos, const instr_base *base) {
	reg rd = b->fresh_vreg(aarch64::x_space, aarch64::x_regs);
	make_cntinc_r(b, instr_pos, rd, base);
	return rd;
}

reg aarch64::instr_builder::make_cntb() {
	return make_cntinc(b, instr_pos, &cntb_r_base);
}

void aarch64::instr_builder::make_cntb_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &cntb_r_base);
}

reg aarch64::instr_builder::make_cnth() {
	return make_cntinc(b, instr_pos, &cnth_r_base);
}

void aarch64::instr_builder::make_cnth_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &cnth_r_base);
}

reg aarch64::instr_builder::make_cntw() {
	return make_cntinc(b, instr_pos, &cntw_r_base);
}

void aarch64::instr_builder::make_cntw_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &cntw_r_base);
}

reg aarch64::instr_builder::make_cntd() {
	return make_cntinc(b, instr_pos, &cntd_r_base);
}

void aarch64::instr_builder::make_cntd_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &cntd_r_base);
}

void aarch64::instr_builder::make_cntp_rpp(reg rd, reg pg, reg pn, z_type_variant zv) {
  reg_assert_classes_equal_to(aarch64::x_regs, rd);
  reg_assert_classes_equal_to(aarch64::p_regs, pg, pn);
  std::vector<reg> regs{ rd, pg, pn };
  std::vector<int64_t> literals{ zv };
  std::vector<branch_target *> targets;
  make_instr(*b, instr_pos, &cntp_rpp_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_incb() {
	return make_cntinc(b, instr_pos, &incb_r_base);
}

void aarch64::instr_builder::make_incb_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &incb_r_base);
}

reg aarch64::instr_builder::make_inch() {
	return make_cntinc(b, instr_pos, &inch_r_base);
}

void aarch64::instr_builder::make_inch_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &inch_r_base);
}

reg aarch64::instr_builder::make_incw() {
	return make_cntinc(b, instr_pos, &incw_r_base);
}

void aarch64::instr_builder::make_incw_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &incw_r_base);
}

reg aarch64::instr_builder::make_incd() {
	return make_cntinc(b, instr_pos, &incd_r_base);
}

void aarch64::instr_builder::make_incd_r(reg rd) {
	return make_cntinc_r(b, instr_pos, rd, &incd_r_base);
}

reg aarch64::instr_builder::make_addvl_ri(reg rn, int imm) {
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	reg rd = b->fresh_vreg(aarch64::x_space, aarch64::x_regs);
	make_addvl_rri(rd, rn, imm);
	return rd;
}

void aarch64::instr_builder::make_addvl_rri(reg rd, reg rn, int imm) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &addvl_rri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_addsvl_ri(reg rn, int imm) {
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	reg rd = b->fresh_vreg(aarch64::x_space, aarch64::x_regs);
	make_addsvl_rri(rd, rn, imm);
	return rd;
}

void aarch64::instr_builder::make_addsvl_rri(reg rd, reg rn, int imm) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &addsvl_rri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_index_ii(int imm, int immb, z_type_variant zv) {
	reg rd = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_index_zii(rd, imm, immb, zv);
	return rd;
}

void aarch64::instr_builder::make_index_zii(reg rd, int imm, int immb, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ imm, immb, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &index_zii_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_index_ir(int imm, reg rm, z_type_variant zv) {
	reg rd = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_index_zir(rd, imm, rm, zv);
	return rd;
}

void aarch64::instr_builder::make_index_zir(reg rd, int imm, reg rm, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd);
	reg_assert_classes_equal_to(aarch64::x_regs, rm);
	std::vector<reg> regs{ rd, rm };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &index_zir_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_index_ri(reg rn, int imm, z_type_variant zv) {
	reg rd = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_index_zri(rd, rn, imm, zv);
	return rd;
}

void aarch64::instr_builder::make_index_zri(reg rd, reg rn, int imm, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &index_zri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_index_rr(reg rn, reg rm, z_type_variant zv) {
	reg rd = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_index_zrr(rd, rn, rm, zv);
	return rd;
}

void aarch64::instr_builder::make_index_zrr(reg rd, reg rn, reg rm, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &index_zrr_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_x_and_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &and_rrr_base);
}

reg aarch64::instr_builder::make_x_and_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &and_rrr_base);
}

void aarch64::instr_builder::make_and_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &and_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_and_qq(reg rn, reg rm, q_type_variant qv) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_and_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_and_zzz(reg rd, reg rn, reg rm) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv_d, &and_zzz_base);
}

reg aarch64::instr_builder::make_and_zz(reg rn, reg rm) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv_d, &and_zzz_base);
}

void aarch64::instr_builder::make_x_orr_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &orr_rrr_base);
}

reg aarch64::instr_builder::make_x_orr_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &orr_rrr_base);
}

void aarch64::instr_builder::make_orr_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &orr_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_orr_qq(reg rn, reg rm, q_type_variant qv) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_orr_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_orr_zzz(reg rd, reg rn, reg rm) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv_d, &orr_zzz_base);
}

reg aarch64::instr_builder::make_orr_zz(reg rn, reg rm) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv_d, &orr_zzz_base);
}

template <int cond>
static void make_csel_rrr(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, const instr_base *base) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ cond };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

template <int cond>
static reg make_csel_rr(block *b, instruction *instr_pos, reg rn, reg rm, const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::x_space, rc);
	make_csel_rrr<cond>(b, instr_pos, rd, rn, rm, base);
	return rd;
}

reg aarch64::instr_builder::make_x_csel_eq_rr(reg rn, reg rm) {
	return make_csel_rr<0b0000>(b, instr_pos, rn, rm, &csel_rrri_base);
}

void aarch64::instr_builder::make_x_csel_eq_rrr(reg rd, reg rn, reg rm) {
	make_csel_rrr<0b0000>(b, instr_pos, rd, rn, rm, &csel_rrri_base);
}

reg aarch64::instr_builder::make_x_csel_ne_rr(reg rn, reg rm) {
	return make_csel_rr<0b0001>(b, instr_pos, rn, rm, &csel_rrri_base);
}

void aarch64::instr_builder::make_x_csel_ne_rrr(reg rd, reg rn, reg rm) {
	make_csel_rrr<0b0001>(b, instr_pos, rd, rn, rm, &csel_rrri_base);
}

reg aarch64::instr_builder::make_x_csel_le_rr(reg rn, reg rm) {
	return make_csel_rr<0b1101>(b, instr_pos, rn, rm, &csel_rrri_base);
}

void aarch64::instr_builder::make_x_csel_le_rrr(reg rd, reg rn, reg rm) {
	make_csel_rrr<0b1101>(b, instr_pos, rd, rn, rm, &csel_rrri_base);
}

reg aarch64::instr_builder::make_x_csel_lt_rr(reg rn, reg rm) {
	return make_csel_rr<0b1011>(b, instr_pos, rn, rm, &csel_rrri_base);
}

void aarch64::instr_builder::make_x_csel_lt_rrr(reg rd, reg rn, reg rm) {
	make_csel_rrr<0b1011>(b, instr_pos, rd, rn, rm, &csel_rrri_base);
}

reg aarch64::instr_builder::make_x_csel_ge_rr(reg rn, reg rm) {
	return make_csel_rr<0b1010>(b, instr_pos, rn, rm, &csel_rrri_base);
}

void aarch64::instr_builder::make_x_csel_ge_rrr(reg rd, reg rn, reg rm) {
	make_csel_rrr<0b1010>(b, instr_pos, rd, rn, rm, &csel_rrri_base);
}

reg aarch64::instr_builder::make_x_csel_gt_rr(reg rn, reg rm) {
	return make_csel_rr<0b1100>(b, instr_pos, rn, rm, &csel_rrri_base);
}

void aarch64::instr_builder::make_x_csel_gt_rrr(reg rd, reg rn, reg rm) {
	make_csel_rrr<0b1100>(b, instr_pos, rd, rn, rm, &csel_rrri_base);
}

reg aarch64::instr_builder::make_rev16_q(reg rn, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_rev16_qq(rd, rn, qv);
	return rd;
}

void aarch64::instr_builder::make_rev16_qq(reg rd, reg rn, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &rev16_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_rev32_q(reg rn, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_rev32_qq(rd, rn, qv);
	return rd;
}

void aarch64::instr_builder::make_rev32_qq(reg rd, reg rn, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &rev32_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_rev64_q(reg rn, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_rev64_qq(rd, rn, qv);
	return rd;
}

void aarch64::instr_builder::make_rev64_qq(reg rd, reg rn, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &rev64_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_revx_zpz(block *b, instruction *instr_pos, reg rd, reg pg, reg rn,
                          aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rd, pg, rn };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_revb_zpz(reg rd, reg pg, reg rn, z_type_variant zv) {
	make_revx_zpz(b, instr_pos, rd, pg, rn, zv, &revb_zpz_base);
}

void aarch64::instr_builder::make_revh_zpz(reg rd, reg pg, reg rn, z_type_variant zv) {
	make_revx_zpz(b, instr_pos, rd, pg, rn, zv, &revh_zpz_base);
}

void aarch64::instr_builder::make_revw_zpz(reg rd, reg pg, reg rn, z_type_variant zv) {
	make_revx_zpz(b, instr_pos, rd, pg, rn, zv, &revw_zpz_base);
}

void aarch64::instr_builder::make_rev_zz(reg rd, reg rn, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &rev_zz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_rev_z(reg rn, z_type_variant zv) {
	reg rd = b->fresh_vreg(aarch64::v_space, rn.active_mask);
	make_rev_zz(rd, rn, zv);
	return rd;
}

void aarch64::instr_builder::make_rev_pp(reg pd, reg pn, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::p_regs, pd, pn);
	std::vector<reg> regs{ pd, pn };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &rev_pp_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_rev_p(reg pn, z_type_variant zv) {
	reg pd = b->fresh_vreg(aarch64::p_space, pn.active_mask);
	make_rev_pp(pd, pn, zv);
	return pd;
}

void aarch64::instr_builder::make_ext_zzi(reg rdn, reg rm, unsigned idx) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn, rm);
	std::vector<reg> regs{ rdn, rm };
	std::vector<int64_t> literals{ idx };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ext_zzi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_ext_qq(reg rn, reg rm, int idx, q_type_variant qv) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(v_space, rc);
	make_ext_qqq(rd, rn, rm, idx, qv);
	return rd;
}

void aarch64::instr_builder::make_ext_qqq(reg rd, reg rn, reg rm, int idx, q_type_variant qv) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ idx, qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ext_qqqi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_uzp_qqq(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::q_type_variant qv,
                         const instr_base *base) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_uzp_qq(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::q_type_variant qv,
                       const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_uzp_qqq(b, instr_pos, rd, rn, rm, qv, base);
	return rd;
}

reg aarch64::instr_builder::make_uzp1_qq(reg rn, reg rm, q_type_variant qv) {
	return make_uzp_qq(b, instr_pos, rn, rm, qv, &uzp1_qqq_base);
}

void aarch64::instr_builder::make_uzp1_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	make_uzp_qqq(b, instr_pos, rd, rn, rm, qv, &uzp1_qqq_base);
}

reg aarch64::instr_builder::make_uzp2_qq(reg rn, reg rm, q_type_variant qv) {
	return make_uzp_qq(b, instr_pos, rn, rm, qv, &uzp2_qqq_base);
}

void aarch64::instr_builder::make_uzp2_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	make_uzp_qqq(b, instr_pos, rd, rn, rm, qv, &uzp2_qqq_base);
}

static void make_uzp_zzz(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::z_type_variant zv,
                         const instr_base *base) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_uzp_zz(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::z_type_variant zv,
                       const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_uzp_zzz(b, instr_pos, rd, rn, rm, zv, base);
	return rd;
}

reg aarch64::instr_builder::make_uzp1_zz(reg rn, reg rm, z_type_variant zv) {
	return make_uzp_zz(b, instr_pos, rn, rm, zv, &uzp1_zzz_base);
}

void aarch64::instr_builder::make_uzp1_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_uzp_zzz(b, instr_pos, rd, rn, rm, zv, &uzp1_zzz_base);
}

reg aarch64::instr_builder::make_uzp2_zz(reg rn, reg rm, z_type_variant zv) {
	return make_uzp_zz(b, instr_pos, rn, rm, zv, &uzp2_zzz_base);
}

void aarch64::instr_builder::make_uzp2_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_uzp_zzz(b, instr_pos, rd, rn, rm, zv, &uzp2_zzz_base);
}

static void make_trn_qqq(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::q_type_variant qv,
                         const instr_base *base) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_trn_qq(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::q_type_variant qv,
                       const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_trn_qqq(b, instr_pos, rd, rn, rm, qv, base);
	return rd;
}

reg aarch64::instr_builder::make_trn1_qq(reg rn, reg rm, q_type_variant qv) {
	return make_trn_qq(b, instr_pos, rn, rm, qv, &trn1_qqq_base);
}

void aarch64::instr_builder::make_trn1_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	make_trn_qqq(b, instr_pos, rd, rn, rm, qv, &trn1_qqq_base);
}

reg aarch64::instr_builder::make_trn2_qq(reg rn, reg rm, q_type_variant qv) {
	return make_trn_qq(b, instr_pos, rn, rm, qv, &trn2_qqq_base);
}

void aarch64::instr_builder::make_trn2_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	make_trn_qqq(b, instr_pos, rd, rn, rm, qv, &trn2_qqq_base);
}

static void make_trn_zzz(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::z_type_variant zv,
                         const instr_base *base) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_trn_zz(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::z_type_variant zv,
                       const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_trn_zzz(b, instr_pos, rd, rn, rm, zv, base);
	return rd;
}

reg aarch64::instr_builder::make_trn1_zz(reg rn, reg rm, z_type_variant zv) {
	return make_trn_zz(b, instr_pos, rn, rm, zv, &trn1_zzz_base);
}

void aarch64::instr_builder::make_trn1_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_trn_zzz(b, instr_pos, rd, rn, rm, zv, &trn1_zzz_base);
}

reg aarch64::instr_builder::make_trn2_zz(reg rn, reg rm, z_type_variant zv) {
	return make_trn_zz(b, instr_pos, rn, rm, zv, &trn2_zzz_base);
}

void aarch64::instr_builder::make_trn2_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_trn_zzz(b, instr_pos, rd, rn, rm, zv, &trn2_zzz_base);
}

static void make_zip_qqq(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::q_type_variant qv,
                         const instr_base *base) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_zip_qq(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::q_type_variant qv,
                       const instr_base *base) {
	reg rd = b->fresh_vreg(aarch64::v_space, get_active_mask_from_qv(qv));
	make_zip_qqq(b, instr_pos, rd, rn, rm, qv, base);
	return rd;
}

reg aarch64::instr_builder::make_zip1_qq(reg rn, reg rm, q_type_variant qv) {
	return make_zip_qq(b, instr_pos, rn, rm, qv, &zip1_qqq_base);
}

void aarch64::instr_builder::make_zip1_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	make_zip_qqq(b, instr_pos, rd, rn, rm, qv, &zip1_qqq_base);
}

reg aarch64::instr_builder::make_zip2_qq(reg rn, reg rm, q_type_variant qv) {
	return make_zip_qq(b, instr_pos, rn, rm, qv, &zip2_qqq_base);
}

void aarch64::instr_builder::make_zip2_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	make_zip_qqq(b, instr_pos, rd, rn, rm, qv, &zip2_qqq_base);
}

static void make_zip_zzz(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, aarch64::z_type_variant zv,
                         const instr_base *base) {
	reg_assert_classes_equal(rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_zip_zz(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::z_type_variant zv,
                       const instr_base *base) {
	auto rc = reg_assert_classes_equal_and_get(rn, rm);
	reg rd = b->fresh_vreg(aarch64::v_space, rc);
	make_zip_zzz(b, instr_pos, rd, rn, rm, zv, base);
	return rd;
}

reg aarch64::instr_builder::make_zip1_zz(reg rn, reg rm, z_type_variant zv) {
	return make_zip_zz(b, instr_pos, rn, rm, zv, &zip1_zzz_base);
}

void aarch64::instr_builder::make_zip1_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_zip_zzz(b, instr_pos, rd, rn, rm, zv, &zip1_zzz_base);
}

reg aarch64::instr_builder::make_zip2_zz(reg rn, reg rm, z_type_variant zv) {
	return make_zip_zz(b, instr_pos, rn, rm, zv, &zip2_zzz_base);
}

void aarch64::instr_builder::make_zip2_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_zip_zzz(b, instr_pos, rd, rn, rm, zv, &zip2_zzz_base);
}

void aarch64::instr_builder::make_splice_zpz(reg rd, reg pg, reg rn, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rd, pg, rn };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &splice_zpz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_op_ppp(block *b, instruction *instr_pos, reg pd, reg pn, reg pm, aarch64::z_type_variant zv,
                        const instr_base *base) {
	reg_assert_classes_equal(pd, pn, pm);
	std::vector<reg> regs{ pd, pn, pm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

[[nodiscard]] static reg make_op_pp(block *b, instruction *instr_pos, reg pn, reg pm,
                                    aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::p_regs, pn, pm);
	reg pd = b->fresh_vreg(aarch64::p_space, aarch64::p_regs);
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, base);
	return pd;
}

reg aarch64::instr_builder::make_trn1_pp(reg pn, reg pm, z_type_variant zv) {
	return make_op_pp(b, instr_pos, pn, pm, zv, &trn1_ppp_base);
}

void aarch64::instr_builder::make_trn1_ppp(reg pd, reg pn, reg pm, z_type_variant zv) {
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, &trn1_ppp_base);
}

reg aarch64::instr_builder::make_trn2_pp(reg pn, reg pm, z_type_variant zv) {
	return make_op_pp(b, instr_pos, pn, pm, zv, &trn2_ppp_base);
}

void aarch64::instr_builder::make_trn2_ppp(reg pd, reg pn, reg pm, z_type_variant zv) {
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, &trn2_ppp_base);
}

reg aarch64::instr_builder::make_uzp1_pp(reg pn, reg pm, z_type_variant zv) {
	return make_op_pp(b, instr_pos, pn, pm, zv, &uzp1_ppp_base);
}

void aarch64::instr_builder::make_uzp1_ppp(reg pd, reg pn, reg pm, z_type_variant zv) {
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, &uzp1_ppp_base);
}

reg aarch64::instr_builder::make_uzp2_pp(reg pn, reg pm, z_type_variant zv) {
	return make_op_pp(b, instr_pos, pn, pm, zv, &uzp2_ppp_base);
}

void aarch64::instr_builder::make_uzp2_ppp(reg pd, reg pn, reg pm, z_type_variant zv) {
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, &uzp2_ppp_base);
}

reg aarch64::instr_builder::make_zip1_pp(reg pn, reg pm, z_type_variant zv) {
	return make_op_pp(b, instr_pos, pn, pm, zv, &zip1_ppp_base);
}

void aarch64::instr_builder::make_zip1_ppp(reg pd, reg pn, reg pm, z_type_variant zv) {
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, &zip1_ppp_base);
}

reg aarch64::instr_builder::make_zip2_pp(reg pn, reg pm, z_type_variant zv) {
	return make_op_pp(b, instr_pos, pn, pm, zv, &zip2_ppp_base);
}

void aarch64::instr_builder::make_zip2_ppp(reg pd, reg pn, reg pm, z_type_variant zv) {
	make_op_ppp(b, instr_pos, pd, pn, pm, zv, &zip2_ppp_base);
}

reg aarch64::instr_builder::make_dup_zi(int imm, z_type_variant zv) {
	reg rd = b->fresh_vreg(v_space, z_regs);
	make_dup_zi(rd, imm, zv);
	return rd;
}

void aarch64::instr_builder::make_dup_zi(reg rd, int imm, z_type_variant zv) {
	reg_assert_classes_equal_to(z_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &dup_zi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_dup_zl(reg rn, int lane, z_type_variant zv) {
	reg_assert_classes_equal_to(z_regs, rn);
	reg rd = b->fresh_vreg(v_space, z_regs);
	make_dup_zzl(rd, rn, lane, zv);
	return rd;
}

void aarch64::instr_builder::make_dup_zzl(reg rd, reg rn, int lane, z_type_variant zv) {
	reg_assert_classes_equal_to(z_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ lane, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &dup_zzl_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_dup_qr(reg rd, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(q_regs, rd);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &dup_qr_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_dup_ql(reg rn, int lane, q_type_variant qv) {
	reg rd = b->fresh_vreg(v_space, get_active_mask_from_qv(qv));
	make_dup_qql(rd, rn, lane, qv);
	return rd;
}

void aarch64::instr_builder::make_dup_qql(reg rd, reg rn, int lane, q_type_variant qv) {
	reg_assert_classes_equal_to(q_regs, rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ lane, qv };
	std::vector<branch_target *> targets;
    make_instr(*b, instr_pos, &dup_qql_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_classes dst_rc, aarch64::preg_classes src_rc>
void make_fcvt_ff(block *b, instruction *instr_pos, reg rd, reg rn, const instr_base *base) {
	reg_assert_classes_equal_to(dst_rc, rd);
	reg_assert_classes_equal_to(src_rc, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_classes dst_rc, aarch64::preg_classes src_rc>
reg make_fcvt_ff(block *b, instruction *instr_pos, reg rn, const instr_base *base) {
	reg rd = b->fresh_vreg(aarch64::v_space, dst_rc);
	make_fcvt_ff<dst_rc, src_rc>(b, instr_pos, rd, rn, base);
	return rd;
}

reg aarch64::instr_builder::make_fcvt_sh(reg rn) {
	return make_fcvt_ff<s_regs, h_regs>(b, instr_pos, rn, &fcvt_sh_base);
}

void aarch64::instr_builder::make_fcvt_sh(reg rd, reg rn) {
	make_fcvt_ff<s_regs, h_regs>(b, instr_pos, rd, rn, &fcvt_sh_base);
}

reg aarch64::instr_builder::make_fcvt_dh(reg rn) {
	return make_fcvt_ff<d_regs, h_regs>(b, instr_pos, rn, &fcvt_dh_base);
}

void aarch64::instr_builder::make_fcvt_dh(reg rd, reg rn) {
	make_fcvt_ff<d_regs, h_regs>(b, instr_pos, rd, rn, &fcvt_dh_base);
}

reg aarch64::instr_builder::make_fcvt_hs(reg rn) {
	return make_fcvt_ff<h_regs, s_regs>(b, instr_pos, rn, &fcvt_hs_base);
}

void aarch64::instr_builder::make_fcvt_hs(reg rd, reg rn) {
	make_fcvt_ff<h_regs, s_regs>(b, instr_pos, rd, rn, &fcvt_hs_base);
}

reg aarch64::instr_builder::make_fcvt_ds(reg rn) {
	return make_fcvt_ff<d_regs, s_regs>(b, instr_pos, rn, &fcvt_ds_base);
}

void aarch64::instr_builder::make_fcvt_ds(reg rd, reg rn) {
	make_fcvt_ff<d_regs, s_regs>(b, instr_pos, rd, rn, &fcvt_ds_base);
}

reg aarch64::instr_builder::make_fcvt_hd(reg rn) {
	return make_fcvt_ff<h_regs, d_regs>(b, instr_pos, rn, &fcvt_hd_base);
}

void aarch64::instr_builder::make_fcvt_hd(reg rd, reg rn) {
	make_fcvt_ff<h_regs, d_regs>(b, instr_pos, rd, rn, &fcvt_hd_base);
}

reg aarch64::instr_builder::make_fcvt_sd(reg rn) {
	return make_fcvt_ff<s_regs, d_regs>(b, instr_pos, rn, &fcvt_sd_base);
}

void aarch64::instr_builder::make_fcvt_sd(reg rd, reg rn) {
	make_fcvt_ff<s_regs, d_regs>(b, instr_pos, rd, rn, &fcvt_sd_base);
}

template <aarch64::preg_classes dst_rc, aarch64::preg_classes src_rc>
void make_fmov_xx(block *b, instruction *instr_pos, reg rd, reg rn, const instr_base *base) {
	reg_assert_classes_equal_to(dst_rc, rd);
	reg_assert_classes_equal_to(src_rc, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_spaces dst_space, aarch64::preg_classes dst_rc, aarch64::preg_classes src_rc>
reg make_fmov_xx(block *b, instruction *instr_pos, reg rn, const instr_base *base) {
	reg rd = b->fresh_vreg(dst_space, dst_rc);
	make_fcvt_ff<dst_rc, src_rc>(b, instr_pos, rd, rn, base);
	return rd;
}

reg aarch64::instr_builder::make_fmov_rh(reg rn) {
	return make_fmov_xx<x_space, x_regs, h_regs>(b, instr_pos, rn, &fmov_rh_base);
}

void aarch64::instr_builder::make_fmov_rh(reg rd, reg rn) {
	make_fmov_xx<x_regs, h_regs>(b, instr_pos, rd, rn, &fmov_rh_base);
}

reg aarch64::instr_builder::make_fmov_rs(reg rn) {
	return make_fmov_xx<x_space, x_regs, s_regs>(b, instr_pos, rn, &fmov_rs_base);
}

void aarch64::instr_builder::make_fmov_rs(reg rd, reg rn) {
	make_fmov_xx<x_regs, s_regs>(b, instr_pos, rd, rn, &fmov_rs_base);
}

reg aarch64::instr_builder::make_fmov_rd(reg rn) {
	return make_fmov_xx<x_space, x_regs, d_regs>(b, instr_pos, rn, &fmov_rd_base);
}

void aarch64::instr_builder::make_fmov_rd(reg rd, reg rn) {
	make_fmov_xx<x_regs, d_regs>(b, instr_pos, rd, rn, &fmov_rd_base);
}

reg aarch64::instr_builder::make_fmov_hr(reg rn) {
	return make_fmov_xx<v_space, h_regs, x_regs>(b, instr_pos, rn, &fmov_hr_base);
}

void aarch64::instr_builder::make_fmov_hr(reg rd, reg rn) {
	make_fmov_xx<h_regs, x_regs>(b, instr_pos, rd, rn, &fmov_hr_base);
}

reg aarch64::instr_builder::make_fmov_sr(reg rn) {
	return make_fmov_xx<v_space, s_regs, x_regs>(b, instr_pos, rn, &fmov_sr_base);
}

void aarch64::instr_builder::make_fmov_sr(reg rd, reg rn) {
	make_fmov_xx<s_regs, x_regs>(b, instr_pos, rd, rn, &fmov_sr_base);
}

reg aarch64::instr_builder::make_fmov_dr(reg rn) {
	return make_fmov_xx<v_space, d_regs, x_regs>(b, instr_pos, rn, &fmov_dr_base);
}

void aarch64::instr_builder::make_fmov_dr(reg rd, reg rn) {
	make_fmov_xx<d_regs, x_regs>(b, instr_pos, rd, rn, &fmov_dr_base);
}

reg aarch64::instr_builder::make_smov_ql(reg rn, int lane, q_type_variant qv) {
	reg rd = b->fresh_vreg(aarch64::x_space, aarch64::x_regs);
	make_smov_rql(rd, rn, lane, qv);
	return rd;
}

void aarch64::instr_builder::make_smov_rql(reg rd, reg rn, int lane, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd);
	sloejit_assert(rn.space_id == aarch64::v_space);
	sloejit_assert(lane >= 0);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ lane, qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smov_rql_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fcvtn_q(reg rn, q_type_variant to, q_type_variant from) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_fcvtn_qq(rd, rn, to, from);
	return rd;
}

void aarch64::instr_builder::make_fcvtn_qq(reg rd, reg rn, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fcvtn_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_fcvtl_q(reg rn, q_type_variant to, q_type_variant from) {
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_fcvtl_qq(rd, rn, to, from);
	return rd;
}

void aarch64::instr_builder::make_fcvtl_qq(reg rd, reg rn, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal(rd, rn);
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &fcvtl_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_pfalse() {
	reg rd = b->fresh_vreg(p_space, p_regs);
	make_pfalse(rd);
	return rd;
}

void aarch64::instr_builder::make_pfalse(reg rd) {
	reg_assert_classes_equal_to(p_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &pfalse_p_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_ptrue(z_type_variant zv) {
	return make_ptrue(aarch64::ptrue_pat_all, zv);
}

reg aarch64::instr_builder::make_ptrue(ptrue_pat pat, z_type_variant zv) {
	reg rd = b->fresh_vreg(p_space, p_regs);
	make_ptrue(rd, pat, zv);
	return rd;
}

void aarch64::instr_builder::make_ptrue(reg rd, z_type_variant zv) {
	make_ptrue(rd, aarch64::ptrue_pat_all, zv);
}

void aarch64::instr_builder::make_ptrue(reg rd, ptrue_pat pat, z_type_variant zv) {
	reg_assert_classes_equal_to(p_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ pat, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ptrue_p_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_whilexx_prr(block *b, instruction *instr_pos, reg pd, reg rn, reg rm,
                             aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pd);
	std::vector<reg> regs{ pd, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_whilexx_rr(block *b, instruction *instr_pos, reg rn, reg rm, aarch64::z_type_variant zv,
                           const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	reg pd = b->fresh_vreg(aarch64::p_space, aarch64::p_regs);
	make_whilexx_prr(b, instr_pos, pd, rn, rm, zv, base);
	return pd;
}

reg aarch64::instr_builder::make_whilelt_rr(reg rn, reg rm, z_type_variant zv) {
	return make_whilexx_rr(b, instr_pos, rn, rm, zv, &whilelt_prr_base);
}

void aarch64::instr_builder::make_whilelt_prr(reg pd, reg rn, reg rm, z_type_variant zv) {
	make_whilexx_prr(b, instr_pos, pd, rn, rm, zv, &whilelt_prr_base);
}

reg aarch64::instr_builder::make_whilele_rr(reg rn, reg rm, z_type_variant zv) {
	return make_whilexx_rr(b, instr_pos, rn, rm, zv, &whilele_prr_base);
}

void aarch64::instr_builder::make_whilele_prr(reg pd, reg rn, reg rm, z_type_variant zv) {
	make_whilexx_prr(b, instr_pos, pd, rn, rm, zv, &whilele_prr_base);
}

void aarch64::instr_builder::make_adr_rb(reg rd, branch_target *bt) {
	reg_assert_classes_equal_to(x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, &adr_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_adr_b(branch_target *bt) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_adr_rb(rd, bt);
	return rd;
}

void aarch64::instr_builder::make_adr_ri(reg rd, int64_t imm) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &adr_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_adr_i(int64_t imm) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_adr_ri(rd, imm);
	return rd;
}

void aarch64::instr_builder::make_adrp_rb(reg rd, branch_target *bt) {
	reg_assert_classes_equal_to(x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, &adrp_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_adrp_b(branch_target *bt) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_adrp_rb(rd, bt);
	return rd;
}

void aarch64::instr_builder::make_adrp_ri(reg rd, int64_t imm) {
	reg_assert_classes_equal_to(aarch64::x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &adrp_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_adrp_i(int64_t imm) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_adrp_ri(rd, imm);
	return rd;
}

void aarch64::instr_builder::make_x_ldrsb_rrr(reg rd, reg rn, reg rm) {
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &ldrsb_x_rrr_base);
}

reg aarch64::instr_builder::make_x_ldrsb_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &ldrsb_x_rrr_base);
}

void aarch64::instr_builder::make_x_ldrsb_rri(reg rd, reg rn, uint32_t imm) {
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &ldrsb_x_rri_base);
}

reg aarch64::instr_builder::make_x_ldrsb_ri(reg rn, uint32_t imm) {
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &ldrsb_x_rri_base);
}

void aarch64::instr_builder::make_x_ldr_rrr(reg rd, reg rn, reg rm) {
	// rm == xzr is legal, but it causes problems because xzr is
	// elided, and LDR defaults to the immediate rather than the
	// register form when there is no offset.
	sloejit_assert(rm != aarch64::xzr);
	make_x_twoarg_rrr(b, instr_pos, rd, rn, rm, &ldr_x_rrr_base);
}

reg aarch64::instr_builder::make_x_ldr_rr(reg rn, reg rm) {
	return make_x_twoarg_rr(b, instr_pos, rn, rm, &ldr_x_rrr_base);
}

void aarch64::instr_builder::make_x_ldr_rri(reg rd, reg rn, uint32_t imm) {
	make_x_twoarg_rri(b, instr_pos, rd, rn, imm, &ldr_x_rri_base);
}

reg aarch64::instr_builder::make_x_ldr_ri(reg rn, uint32_t imm) {
	return make_x_twoarg_ri(b, instr_pos, rn, imm, &ldr_x_rri_base);
}

void aarch64::instr_builder::make_x_ldp_postindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rt1, rt2, rn);
	sloejit_assert(rt1 != rt2);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldp_x_postindex_rrri_base, tag, std::move(regs), std::move(literals),
	           std::move(targets));
}

void aarch64::instr_builder::make_x_ldp_preindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rt1, rt2, rn);
	sloejit_assert(rt1 != rt2);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldp_x_preindex_rrri_base, tag, std::move(regs), std::move(literals),
	           std::move(targets));
}

void aarch64::instr_builder::make_x_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rt1, rt2, rn);
	sloejit_assert(rt1 != rt2);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldp_x_rrri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

std::pair<reg, reg> aarch64::instr_builder::make_s_ldp_ri(reg rn, int32_t imm) {
	reg rt1 = b->fresh_vreg(aarch64::v_space, aarch64::s_regs);
	reg rt2 = b->fresh_vreg(aarch64::v_space, aarch64::s_regs);
	make_s_ldp_rrri(rt1, rt2, rn, imm);
	return { rt1, rt2 };
}

void aarch64::instr_builder::make_s_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	sloejit_assert(rt1 != rt2);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldp_s_ssri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

std::pair<reg, reg> aarch64::instr_builder::make_d_ldp_ri(reg rn, int32_t imm) {
	reg rt1 = b->fresh_vreg(aarch64::v_space, aarch64::d_regs);
	reg rt2 = b->fresh_vreg(aarch64::v_space, aarch64::d_regs);
	make_d_ldp_rrri(rt1, rt2, rn, imm);
	return { rt1, rt2 };
}

void aarch64::instr_builder::make_d_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	sloejit_assert(rt1 != rt2);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldp_d_ddri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

std::pair<reg, reg> aarch64::instr_builder::make_q_ldp_ri(reg rn, int32_t imm) {
	reg rt1 = b->fresh_vreg(aarch64::v_space, aarch64::q_regs);
	reg rt2 = b->fresh_vreg(aarch64::v_space, aarch64::q_regs);
	make_q_ldp_rrri(rt1, rt2, rn, imm);
	return { rt1, rt2 };
}

void aarch64::instr_builder::make_q_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	sloejit_assert(rt1 != rt2);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldp_q_qqri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_x_movz_i(uint64_t imm) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_x_movz_ri(rd, imm);
	return rd;
}

void aarch64::instr_builder::make_x_movz_ri(reg rd, uint64_t imm) {
	reg_assert_classes_equal_to(x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ (int64_t) imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &movz_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_x_movn_i(uint64_t imm) {
	reg rd = b->fresh_vreg(x_space, x_regs);
	make_x_movn_ri(rd, imm);
	return rd;
}

void aarch64::instr_builder::make_x_movn_ri(reg rd, uint64_t imm) {
	reg_assert_classes_equal_to(x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ (int64_t) imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &movn_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_x_movk_ri(reg rd, uint64_t imm) {
	reg_assert_classes_equal_to(x_regs, rd);
	std::vector<reg> regs{ rd };
	std::vector<int64_t> literals{ (int64_t) imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &movk_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_mova_zpalorl(reg zd, reg pg, reg za, uint32_t tile, uint32_t hv, reg rs, uint32_t off, z_type_variant zv) {
	reg_assert_classes_equal_to(za_regs, za);
	reg_assert_classes_equal_to(x_regs, rs);
	reg_assert_classes_equal_to(z_regs, zd);
	reg_assert_classes_equal_to(p_regs, pg);
	std::vector<reg> regs{ zd, pg, za, rs };
	std::vector<int64_t> literals{ tile, hv, off, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &mova_zpalorl_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_mova_alorlpz(reg za, uint32_t tile, uint32_t hv, reg rs, uint32_t off, reg pg, reg zn, z_type_variant zv) {
	reg_assert_classes_equal_to(za_regs, za);
	reg_assert_classes_equal_to(x_regs, rs);
	reg_assert_classes_equal_to(z_regs, zn);
	reg_assert_classes_equal_to(p_regs, pg);
	std::vector<reg> regs{ za, rs, pg, zn };
	std::vector<int64_t> literals{ tile, hv, off, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &mova_alorlpz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_x_strx_rrr(block *b, instruction *instr_pos, reg rt, reg rn, reg rm,
                            const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rt, rn, rm);
	std::vector<reg> regs{ rt, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static void make_x_strx_rri(block *b, instruction *instr_pos, reg rt, reg rn, uint32_t imm,
                            const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::x_regs, rt, rn);
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_x_strb_rrr(reg rt, reg rn, reg rm) {
	return make_x_strx_rrr(b, instr_pos, rt, rn, rm, &strb_x_rrr_base);
}

void aarch64::instr_builder::make_x_strb_rri(reg rt, reg rn, uint32_t imm) {
	return make_x_strx_rri(b, instr_pos, rt, rn, imm, &strb_x_rri_base);
}

void aarch64::instr_builder::make_x_str_rrr(reg rt, reg rn, reg rm) {
	return make_x_strx_rrr(b, instr_pos, rt, rn, rm, &str_x_rrr_base);
}

void aarch64::instr_builder::make_x_str_rri(reg rt, reg rn, uint32_t imm) {
	return make_x_strx_rri(b, instr_pos, rt, rn, imm, &str_x_rri_base);
}

void aarch64::instr_builder::make_x_stp_postindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rt1, rt2, rn);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &stp_x_postindex_rrri_base, tag, std::move(regs), std::move(literals),
	           std::move(targets));
}

void aarch64::instr_builder::make_x_stp_preindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rt1, rt2, rn);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &stp_x_preindex_rrri_base, tag, std::move(regs), std::move(literals),
	           std::move(targets));
}

void aarch64::instr_builder::make_x_stp_rrri(reg rt1, reg rt2, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rt1, rt2, rn);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &stp_x_rrri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_classes rt_class>
static void make_f_ldr_rrr(block *b, instruction *instr_pos, reg rt, reg rn, reg rm, const instr_base *base) {
	reg_assert_classes_equal_to(rt_class, rt);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	std::vector<reg> regs{ rt, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_classes rt_class>
static reg make_f_ldr_rr(block *b, instruction *instr_pos, reg rn, reg rm, const instr_base *base) {
	reg rt = b->fresh_vreg(aarch64::v_space, rt_class);
	make_f_ldr_rrr<rt_class>(b, instr_pos, rt, rn, rm, base);
	return rt;
}

template <aarch64::preg_classes rt_class, uint32_t imm_shift>
static void make_f_ldr_rri(block *b, instruction *instr_pos, reg rt, reg rn, uint32_t imm,
                           const instr_base *base, const std::optional<std::string>& tag) {
	reg_assert_classes_equal_to(rt_class, rt);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	sloejit_assert(imm % (1 << imm_shift) == 0);
	sloejit_assert(imm < (1 << (imm_shift + 12)));
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_classes rt_class, uint32_t imm_shift>
static reg make_f_ldr_ri(block *b, instruction *instr_pos, reg rn, uint32_t imm, const instr_base *base, const std::optional<std::string>& tag) {
	reg rt = b->fresh_vreg(aarch64::v_space, rt_class);
	make_f_ldr_rri<rt_class, imm_shift>(b, instr_pos, rt, rn, imm, base, tag);
	return rt;
}

reg aarch64::instr_builder::make_b_ldr_rr(reg rn, reg rm) {
	return make_f_ldr_rr<b_regs>(b, instr_pos, rn, rm, &ldr_b_brr_base);
}

void aarch64::instr_builder::make_b_ldr_rrr(reg rt, reg rn, reg rm) {
	return make_f_ldr_rrr<b_regs>(b, instr_pos, rt, rn, rm, &ldr_b_brr_base);
}

reg aarch64::instr_builder::make_b_ldr_ri(reg rn, uint32_t imm) {
	return make_f_ldr_ri<b_regs, 0>(b, instr_pos, rn, imm, &ldr_b_bri_base, tag);
}

void aarch64::instr_builder::make_b_ldr_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_ldr_rri<b_regs, 0>(b, instr_pos, rt, rn, imm, &ldr_b_bri_base, tag);
}

reg aarch64::instr_builder::make_h_ldr_rr(reg rn, reg rm) {
	return make_f_ldr_rr<h_regs>(b, instr_pos, rn, rm, &ldr_h_hrr_base);
}

void aarch64::instr_builder::make_h_ldr_rrr(reg rt, reg rn, reg rm) {
	return make_f_ldr_rrr<h_regs>(b, instr_pos, rt, rn, rm, &ldr_h_hrr_base);
}

reg aarch64::instr_builder::make_h_ldr_ri(reg rn, uint32_t imm) {
	return make_f_ldr_ri<h_regs, 1>(b, instr_pos, rn, imm, &ldr_h_hri_base, tag);
}

void aarch64::instr_builder::make_h_ldr_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_ldr_rri<h_regs, 1>(b, instr_pos, rt, rn, imm, &ldr_h_hri_base, tag);
}

reg aarch64::instr_builder::make_s_ldr_rr(reg rn, reg rm) {
	return make_f_ldr_rr<s_regs>(b, instr_pos, rn, rm, &ldr_s_srr_base);
}

void aarch64::instr_builder::make_s_ldr_rrr(reg rt, reg rn, reg rm) {
	return make_f_ldr_rrr<s_regs>(b, instr_pos, rt, rn, rm, &ldr_s_srr_base);
}

reg aarch64::instr_builder::make_s_ldr_ri(reg rn, uint32_t imm) {
	return make_f_ldr_ri<s_regs, 2>(b, instr_pos, rn, imm, &ldr_s_sri_base, tag);
}

void aarch64::instr_builder::make_s_ldr_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_ldr_rri<s_regs, 2>(b, instr_pos, rt, rn, imm, &ldr_s_sri_base, tag);
}

reg aarch64::instr_builder::make_d_ldr_rr(reg rn, reg rm) {
	return make_f_ldr_rr<d_regs>(b, instr_pos, rn, rm, &ldr_d_drr_base);
}

void aarch64::instr_builder::make_d_ldr_rrr(reg rt, reg rn, reg rm) {
	return make_f_ldr_rrr<d_regs>(b, instr_pos, rt, rn, rm, &ldr_d_drr_base);
}

reg aarch64::instr_builder::make_d_ldr_ri(reg rn, uint32_t imm) {
	return make_f_ldr_ri<d_regs, 3>(b, instr_pos, rn, imm, &ldr_d_dri_base, tag);
}

void aarch64::instr_builder::make_d_ldr_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_ldr_rri<d_regs, 3>(b, instr_pos, rt, rn, imm, &ldr_d_dri_base, tag);
}

reg aarch64::instr_builder::make_q_ldr_rr(reg rn, reg rm) {
	return make_f_ldr_rr<q_regs>(b, instr_pos, rn, rm, &ldr_q_qrr_base);
}

void aarch64::instr_builder::make_q_ldr_rrr(reg rt, reg rn, reg rm) {
	return make_f_ldr_rrr<q_regs>(b, instr_pos, rt, rn, rm, &ldr_q_qrr_base);
}

reg aarch64::instr_builder::make_q_ldr_ri(reg rn, uint32_t imm) {
	return make_f_ldr_ri<q_regs, 4>(b, instr_pos, rn, imm, &ldr_q_qri_base, tag);
}

void aarch64::instr_builder::make_q_ldr_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_ldr_rri<q_regs, 4>(b, instr_pos, rt, rn, imm, &ldr_q_qri_base, tag);
}

reg aarch64::instr_builder::make_ld1r_r(reg rn, q_type_variant qv) {
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::q_regs);
	make_ld1r_rr(rt, rn, qv);
	return rt;
}

void aarch64::instr_builder::make_ld1r_rr(reg rt, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ld1r_qr_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_ld2_rrr(reg rt1, reg rt2, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt1, rt2);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 2 } };
	make_instr(*b, instr_pos, &ld2_qqr_base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

void aarch64::instr_builder::make_ld3_rrrr(reg rt1, reg rt2, reg rt3, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt1, rt2, rt3);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt1, rt2, rt3, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 3 } };
	make_instr(*b, instr_pos, &ld3_qqqr_base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

void aarch64::instr_builder::make_ld4_rrrrr(reg rt1, reg rt2, reg rt3, reg rt4, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt1, rt2, rt3, rt4);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt1, rt2, rt3, rt4, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 4 } };
	make_instr(*b, instr_pos, &ld4_qqqqr_base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

void aarch64::instr_builder::make_st2_rrr(reg rt1, reg rt2, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt1, rt2);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt1, rt2, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 2 } };
	make_instr(*b, instr_pos, &st2_qqr_base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

void aarch64::instr_builder::make_st3_rrrr(reg rt1, reg rt2, reg rt3, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt1, rt2, rt3);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt1, rt2, rt3, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 3 } };
	make_instr(*b, instr_pos, &st3_qqqr_base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

void aarch64::instr_builder::make_st4_rrrrr(reg rt1, reg rt2, reg rt3, reg rt4, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt1, rt2, rt3, rt4);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt1, rt2, rt3, rt4, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 4 } };
	make_instr(*b, instr_pos, &st4_qqqqr_base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

template <aarch64::preg_classes rt_class>
static void make_f_str_rrr(block *b, instruction *instr_pos, reg rt, reg rn, reg rm, const instr_base *base, const std::optional<std::string>& tag) {
	reg_assert_classes_equal_to(rt_class, rt);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	std::vector<reg> regs{ rt, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets));
}

template <aarch64::preg_classes rt_class, uint32_t imm_mod>
static void make_f_str_rri(block *b, instruction *instr_pos, reg rt, reg rn, uint32_t imm,
                           const instr_base *base, const std::optional<std::string>& tag) {
	reg_assert_classes_equal_to(rt_class, rt);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	sloejit_assert(imm % imm_mod == 0);
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_b_str_rrr(reg rt, reg rn, reg rm) {
	return make_f_str_rrr<b_regs>(b, instr_pos, rt, rn, rm, &str_b_brr_base, tag);
}

void aarch64::instr_builder::make_b_str_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_str_rri<b_regs, 1>(b, instr_pos, rt, rn, imm, &str_b_bri_base, tag);
}

void aarch64::instr_builder::make_h_str_rrr(reg rt, reg rn, reg rm) {
	return make_f_str_rrr<h_regs>(b, instr_pos, rt, rn, rm, &str_h_hrr_base, tag);
}

void aarch64::instr_builder::make_h_str_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_str_rri<h_regs, 2>(b, instr_pos, rt, rn, imm, &str_h_hri_base, tag);
}

void aarch64::instr_builder::make_s_str_rrr(reg rt, reg rn, reg rm) {
	return make_f_str_rrr<s_regs>(b, instr_pos, rt, rn, rm, &str_s_srr_base, tag);
}

void aarch64::instr_builder::make_s_str_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_str_rri<s_regs, 4>(b, instr_pos, rt, rn, imm, &str_s_sri_base, tag);
}

void aarch64::instr_builder::make_d_str_rrr(reg rt, reg rn, reg rm) {
	return make_f_str_rrr<d_regs>(b, instr_pos, rt, rn, rm, &str_d_drr_base, tag);
}

void aarch64::instr_builder::make_d_str_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_str_rri<d_regs, 8>(b, instr_pos, rt, rn, imm, &str_d_dri_base, tag);
}

void aarch64::instr_builder::make_q_str_rrr(reg rt, reg rn, reg rm) {
	return make_f_str_rrr<q_regs>(b, instr_pos, rt, rn, rm, &str_q_qrr_base, tag);
}

void aarch64::instr_builder::make_q_str_rri(reg rt, reg rn, uint32_t imm) {
	return make_f_str_rri<q_regs, 16>(b, instr_pos, rt, rn, imm, &str_q_qri_base, tag);
}

reg aarch64::instr_builder::make_ldr_zri(reg rn, int32_t imm) {
	reg_assert_classes_equal_to(x_regs, rn);
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_ldr_zri(rt, rn, imm);
	return rt;
}

void aarch64::instr_builder::make_ldr_zri(reg rt, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(z_regs, rt);
	reg_assert_classes_equal_to(x_regs, rn);
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ldr_zri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_str_zri(reg rt, reg rn, int32_t imm) {
	reg_assert_classes_equal_to(z_regs, rt);
	reg_assert_classes_equal_to(x_regs, rn);
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &str_zri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_q_st1_rir(reg rt, int idx, reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rt);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, rn };
	std::vector<int64_t> literals{ idx, qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &st1_q_qir_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_ld1x_zpri(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, int32_t imm,
                           aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, pg, rn };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_ld1x_pri(block *b, instruction *instr_pos, reg pg, reg rn, int32_t imm,
                         aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_ld1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, base);
	return rt;
}

reg aarch64::instr_builder::make_ld1b_pri(reg pg, reg rn, int32_t imm, z_type_variant zv) {
	return make_ld1x_pri(b, instr_pos, pg, rn, imm, zv, &ld1b_zpri_base);
}

void aarch64::instr_builder::make_ld1b_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_ld1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &ld1b_zpri_base);
}

reg aarch64::instr_builder::make_ld1h_pri(reg pg, reg rn, int32_t imm, z_type_variant zv) {
	return make_ld1x_pri(b, instr_pos, pg, rn, imm, zv, &ld1h_zpri_base);
}

void aarch64::instr_builder::make_ld1h_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_ld1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &ld1h_zpri_base);
}

reg aarch64::instr_builder::make_ld1w_pri(reg pg, reg rn, int32_t imm, z_type_variant zv) {
	return make_ld1x_pri(b, instr_pos, pg, rn, imm, zv, &ld1w_zpri_base);
}

void aarch64::instr_builder::make_ld1w_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_ld1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &ld1w_zpri_base);
}

reg aarch64::instr_builder::make_ld1d_pri(reg pg, reg rn, int32_t imm, z_type_variant zv) {
	return make_ld1x_pri(b, instr_pos, pg, rn, imm, zv, &ld1d_zpri_base);
}

void aarch64::instr_builder::make_ld1d_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_ld1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &ld1d_zpri_base);
}

static void make_ld1x_zprr(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, reg rm,
                           aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	std::vector<reg> regs{ rt, pg, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_ld1x_prr(block *b, instruction *instr_pos, reg pg, reg rn, reg rm, aarch64::z_type_variant zv,
                         const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_ld1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, base);
	return rt;
}

reg aarch64::instr_builder::make_ld1b_prr(reg pg, reg rn, reg rm, z_type_variant zv) {
	return make_ld1x_prr(b, instr_pos, pg, rn, rm, zv, &ld1b_zprr_base);
}

void aarch64::instr_builder::make_ld1b_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_ld1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &ld1b_zprr_base);
}

reg aarch64::instr_builder::make_ld1h_prr(reg pg, reg rn, reg rm, z_type_variant zv) {
	return make_ld1x_prr(b, instr_pos, pg, rn, rm, zv, &ld1h_zprr_base);
}

void aarch64::instr_builder::make_ld1h_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_ld1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &ld1h_zprr_base);
}

reg aarch64::instr_builder::make_ld1w_prr(reg pg, reg rn, reg rm, z_type_variant zv) {
	return make_ld1x_prr(b, instr_pos, pg, rn, rm, zv, &ld1w_zprr_base);
}

void aarch64::instr_builder::make_ld1w_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_ld1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &ld1w_zprr_base);
}

reg aarch64::instr_builder::make_ld1d_prr(reg pg, reg rn, reg rm, z_type_variant zv) {
	return make_ld1x_prr(b, instr_pos, pg, rn, rm, zv, &ld1d_zprr_base);
}

void aarch64::instr_builder::make_ld1d_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_ld1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &ld1d_zprr_base);
}

static void make_ld1x_zprz(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, reg rm,
                           aarch64::shift_amount sh, aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, pg, rn, rm };
	std::vector<int64_t> literals{ sh, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_ld1x_prz(block *b, instruction *instr_pos, reg pg, reg rn, reg rm, aarch64::shift_amount sh,
                         aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	reg_assert_classes_equal_to(aarch64::z_regs, rm);
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_ld1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, base);
	return rt;
}

reg aarch64::instr_builder::make_ld1b_prz(reg pg, reg rn, reg rm, shift_amount sh, z_type_variant zv) {
	return make_ld1x_prz(b, instr_pos, pg, rn, rm, sh, zv, &ld1b_zprz_base);
}

void aarch64::instr_builder::make_ld1b_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_ld1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &ld1b_zprz_base);
}

reg aarch64::instr_builder::make_ld1h_prz(reg pg, reg rn, reg rm, shift_amount sh, z_type_variant zv) {
	return make_ld1x_prz(b, instr_pos, pg, rn, rm, sh, zv, &ld1h_zprz_base);
}

void aarch64::instr_builder::make_ld1h_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_ld1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &ld1h_zprz_base);
}

reg aarch64::instr_builder::make_ld1w_prz(reg pg, reg rn, reg rm, shift_amount sh, z_type_variant zv) {
	return make_ld1x_prz(b, instr_pos, pg, rn, rm, sh, zv, &ld1w_zprz_base);
}

void aarch64::instr_builder::make_ld1w_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_ld1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &ld1w_zprz_base);
}

reg aarch64::instr_builder::make_ld1d_prz(reg pg, reg rn, reg rm, shift_amount sh, z_type_variant zv) {
	return make_ld1x_prz(b, instr_pos, pg, rn, rm, sh, zv, &ld1d_zprz_base);
}

void aarch64::instr_builder::make_ld1d_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_ld1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &ld1d_zprz_base);
}

static void make_ldst1x_alorlprr(block *b, instruction *instr_pos, reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off, reg pg, reg rn, reg rm, const instr_base *base){
	reg_assert_classes_equal_to(aarch64::za_regs, za);
	reg_assert_classes_equal_to(aarch64::x_regs, rs, rn, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ za, rs, pg, rn, rm };
	std::vector<int64_t> literals{ tile, v, off };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_ld1b_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off4, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, off4, pg, rn, rm, &ld1b_alorlprr_base);
}

void aarch64::instr_builder::make_ld1h_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off3, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, off3, pg, rn, rm, &ld1h_alorlprr_base);
}

void aarch64::instr_builder::make_ld1w_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off3, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, off3, pg, rn, rm, &ld1w_alorlprr_base);
}

void aarch64::instr_builder::make_ld1d_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &ld1d_alorlprr_base);
}

void aarch64::instr_builder::make_ld1q_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &ld1q_alorlprr_base);
}

void aarch64::instr_builder::make_ld1b_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off4, reg pg, reg rn){
	make_ld1b_alorlprr(za, tile, v, rs, off4, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_ld1h_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off3, reg pg, reg rn){
	make_ld1h_alorlprr(za, tile, v, rs, off3, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_ld1w_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t off3, reg pg, reg rn){
	make_ld1w_alorlprr(za, tile, v, rs, off3, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_ld1d_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_ld1d_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_ld1q_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_ld1q_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_st1b_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &st1b_alorlprr_base);
}

void aarch64::instr_builder::make_st1b_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_st1b_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_st1h_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &st1h_alorlprr_base);
}

void aarch64::instr_builder::make_st1h_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_st1h_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_st1w_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &st1w_alorlprr_base);
}

void aarch64::instr_builder::make_st1w_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_st1w_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_st1d_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &st1d_alorlprr_base);
}

void aarch64::instr_builder::make_st1d_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_st1d_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

void aarch64::instr_builder::make_st1q_alorlprr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm){
	make_ldst1x_alorlprr(b, instr_pos, za, tile, v, rs, o1, pg, rn, rm, &st1q_alorlprr_base);
}

void aarch64::instr_builder::make_st1q_alorlpr(reg za, uint32_t tile, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn){
	make_st1q_alorlprr(za, tile, v, rs, o1, pg, rn, aarch64::xzr);
}

static void make_ld1rx_zpri(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, int32_t imm,
                            const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, pg, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_ld1rx_pri(block *b, instruction *instr_pos, reg pg, reg rn, int32_t imm,
                          const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_ld1rx_zpri(b, instr_pos, rt, pg, rn, imm, base);
	return rt;
}

reg aarch64::instr_builder::make_ld1rb_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rx_pri(b, instr_pos, pg, rn, imm, &ld1rb_zpri_base);
}

void aarch64::instr_builder::make_ld1rb_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rb_zpri_base);
}

reg aarch64::instr_builder::make_ld1rh_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rx_pri(b, instr_pos, pg, rn, imm, &ld1rh_zpri_base);
}

void aarch64::instr_builder::make_ld1rh_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rh_zpri_base);
}

reg aarch64::instr_builder::make_ld1rw_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rx_pri(b, instr_pos, pg, rn, imm, &ld1rw_zpri_base);
}

void aarch64::instr_builder::make_ld1rw_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rw_zpri_base);
}

reg aarch64::instr_builder::make_ld1rd_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rx_pri(b, instr_pos, pg, rn, imm, &ld1rd_zpri_base);
}

void aarch64::instr_builder::make_ld1rd_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rd_zpri_base);
}

static void make_ld1rqx_zpri(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, int32_t imm,
                             const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, pg, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

static reg make_ld1rqx_pri(block *b, instruction *instr_pos, reg pg, reg rn, int32_t imm,
                           const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	reg rt = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_ld1rqx_zpri(b, instr_pos, rt, pg, rn, imm, base);
	return rt;
}

reg aarch64::instr_builder::make_ld1rqb_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_pri(b, instr_pos, pg, rn, imm, &ld1rqb_zpri_base);
}

void aarch64::instr_builder::make_ld1rqb_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rqb_zpri_base);
}

reg aarch64::instr_builder::make_ld1rqh_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_pri(b, instr_pos, pg, rn, imm, &ld1rqh_zpri_base);
}

void aarch64::instr_builder::make_ld1rqh_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rqh_zpri_base);
}

reg aarch64::instr_builder::make_ld1rqw_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_pri(b, instr_pos, pg, rn, imm, &ld1rqw_zpri_base);
}

void aarch64::instr_builder::make_ld1rqw_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rqw_zpri_base);
}

reg aarch64::instr_builder::make_ld1rqd_pri(reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_pri(b, instr_pos, pg, rn, imm, &ld1rqd_zpri_base);
}

void aarch64::instr_builder::make_ld1rqd_zpri(reg rt, reg pg, reg rn, int32_t imm) {
	return make_ld1rqx_zpri(b, instr_pos, rt, pg, rn, imm, &ld1rqd_zpri_base);
}

static void make_ld2_zzpri(block *b, instruction *instr_pos, const std::optional<std::string> &tag,
                           reg zt1, reg zt2, reg pg, reg rn, int32_t imm, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, zt1, zt2);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	if (zt1.id <= aarch64::max_preg_num && zt2.id <= aarch64::max_preg_num) {
		const auto zt1_index = zt1.id - aarch64::z0.id;
		const auto zt2_index = zt2.id - aarch64::z0.id;
		sloejit_assert(zt2_index == (zt1_index + 1) % 32);
	}
	std::vector<reg> regs{ zt1, zt2, pg, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 2 } };
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

static void make_ld2_zzprr(block *b, instruction *instr_pos, const std::optional<std::string> &tag,
                           reg zt1, reg zt2, reg pg, reg rn, reg rm, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, zt1, zt2);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	if (zt1.id <= aarch64::max_preg_num && zt2.id <= aarch64::max_preg_num) {
		const auto zt1_index = zt1.id - aarch64::z0.id;
		const auto zt2_index = zt2.id - aarch64::z0.id;
		sloejit_assert(zt2_index == (zt1_index + 1) % 32);
	}
	std::vector<reg> regs{ zt1, zt2, pg, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 2 } };
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

static void make_ld3_zzzpri(block *b, instruction *instr_pos, const std::optional<std::string> &tag,
                            reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, zt1, zt2, zt3);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	if (zt1.id <= aarch64::max_preg_num && zt3.id <= aarch64::max_preg_num) {
		const auto zt1_index = zt1.id - aarch64::z0.id;
		const auto zt2_index = zt2.id - aarch64::z0.id;
		const auto zt3_index = zt3.id - aarch64::z0.id;
		sloejit_assert(zt2_index == (zt1_index + 1) % 32);
		sloejit_assert(zt3_index == (zt1_index + 2) % 32);
	}
	std::vector<reg> regs{ zt1, zt2, zt3, pg, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 3 } };
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

static void make_ld3_zzzprr(block *b, instruction *instr_pos, const std::optional<std::string> &tag,
                            reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, zt1, zt2, zt3);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	if (zt1.id <= aarch64::max_preg_num && zt3.id <= aarch64::max_preg_num) {
		const auto zt1_index = zt1.id - aarch64::z0.id;
		const auto zt2_index = zt2.id - aarch64::z0.id;
		const auto zt3_index = zt3.id - aarch64::z0.id;
		sloejit_assert(zt2_index == (zt1_index + 1) % 32);
		sloejit_assert(zt3_index == (zt1_index + 2) % 32);
	}
	std::vector<reg> regs{ zt1, zt2, zt3, pg, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 3 } };
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

static void make_ld4_zzzzpri(block *b, instruction *instr_pos, const std::optional<std::string> &tag,
                             reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm,
                             const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, zt1, zt2, zt3, zt4);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	if (zt1.id <= aarch64::max_preg_num && zt4.id <= aarch64::max_preg_num) {
		const auto zt1_index = zt1.id - aarch64::z0.id;
		const auto zt2_index = zt2.id - aarch64::z0.id;
		const auto zt3_index = zt3.id - aarch64::z0.id;
		const auto zt4_index = zt4.id - aarch64::z0.id;
		sloejit_assert(zt2_index == (zt1_index + 1) % 32);
		sloejit_assert(zt3_index == (zt1_index + 2) % 32);
		sloejit_assert(zt4_index == (zt1_index + 3) % 32);
	}
	std::vector<reg> regs{ zt1, zt2, zt3, zt4, pg, rn };
	std::vector<int64_t> literals{ imm };
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 4 } };
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

static void make_ld4_zzzzprr(block *b, instruction *instr_pos, const std::optional<std::string> &tag,
                             reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm,
                             const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, zt1, zt2, zt3, zt4);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	if (zt1.id <= aarch64::max_preg_num && zt4.id <= aarch64::max_preg_num) {
		const auto zt1_index = zt1.id - aarch64::z0.id;
		const auto zt2_index = zt2.id - aarch64::z0.id;
		const auto zt3_index = zt3.id - aarch64::z0.id;
		const auto zt4_index = zt4.id - aarch64::z0.id;
		sloejit_assert(zt2_index == (zt1_index + 1) % 32);
		sloejit_assert(zt3_index == (zt1_index + 2) % 32);
		sloejit_assert(zt4_index == (zt1_index + 3) % 32);
	}
	std::vector<reg> regs{ zt1, zt2, zt3, zt4, pg, rn, rm };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	std::vector<sloejit::reg_sequence> reg_sequences{ { 0, 4 } };
	make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(literals), std::move(targets),
	           std::move(reg_sequences));
}

void aarch64::instr_builder::make_ld2b_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &ld2b_zzpri_base);
}

void aarch64::instr_builder::make_ld2b_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &ld2b_zzprr_base);
}

void aarch64::instr_builder::make_ld2h_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &ld2h_zzpri_base);
}

void aarch64::instr_builder::make_ld2h_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &ld2h_zzprr_base);
}

void aarch64::instr_builder::make_ld2w_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &ld2w_zzpri_base);
}

void aarch64::instr_builder::make_ld2w_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &ld2w_zzprr_base);
}

void aarch64::instr_builder::make_ld2d_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &ld2d_zzpri_base);
}

void aarch64::instr_builder::make_ld2d_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &ld2d_zzprr_base);
}

void aarch64::instr_builder::make_ld2q_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &ld2q_zzpri_base);
}

void aarch64::instr_builder::make_ld2q_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &ld2q_zzprr_base);
}

void aarch64::instr_builder::make_ld3b_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &ld3b_zzzpri_base);
}

void aarch64::instr_builder::make_ld3b_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &ld3b_zzzprr_base);
}

void aarch64::instr_builder::make_ld3h_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &ld3h_zzzpri_base);
}

void aarch64::instr_builder::make_ld3h_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &ld3h_zzzprr_base);
}

void aarch64::instr_builder::make_ld3w_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &ld3w_zzzpri_base);
}

void aarch64::instr_builder::make_ld3w_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &ld3w_zzzprr_base);
}

void aarch64::instr_builder::make_ld3d_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &ld3d_zzzpri_base);
}

void aarch64::instr_builder::make_ld3d_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &ld3d_zzzprr_base);
}

void aarch64::instr_builder::make_ld3q_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &ld3q_zzzpri_base);
}

void aarch64::instr_builder::make_ld3q_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &ld3q_zzzprr_base);
}

void aarch64::instr_builder::make_ld4b_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &ld4b_zzzzpri_base);
}

void aarch64::instr_builder::make_ld4b_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &ld4b_zzzzprr_base);
}

void aarch64::instr_builder::make_ld4h_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &ld4h_zzzzpri_base);
}

void aarch64::instr_builder::make_ld4h_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &ld4h_zzzzprr_base);
}

void aarch64::instr_builder::make_ld4w_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &ld4w_zzzzpri_base);
}

void aarch64::instr_builder::make_ld4w_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &ld4w_zzzzprr_base);
}

void aarch64::instr_builder::make_ld4d_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &ld4d_zzzzpri_base);
}

void aarch64::instr_builder::make_ld4d_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &ld4d_zzzzprr_base);
}

void aarch64::instr_builder::make_ld4q_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &ld4q_zzzzpri_base);
}

void aarch64::instr_builder::make_ld4q_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &ld4q_zzzzprr_base);
}

void aarch64::instr_builder::make_st2b_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &st2b_zzpri_base);
}

void aarch64::instr_builder::make_st2b_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &st2b_zzprr_base);
}

void aarch64::instr_builder::make_st2h_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &st2h_zzpri_base);
}

void aarch64::instr_builder::make_st2h_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &st2h_zzprr_base);
}

void aarch64::instr_builder::make_st2w_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &st2w_zzpri_base);
}

void aarch64::instr_builder::make_st2w_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &st2w_zzprr_base);
}

void aarch64::instr_builder::make_st2d_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &st2d_zzpri_base);
}

void aarch64::instr_builder::make_st2d_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &st2d_zzprr_base);
}

void aarch64::instr_builder::make_st2q_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm) {
	make_ld2_zzpri(b, instr_pos, tag, zt1, zt2, pg, rn, imm, &st2q_zzpri_base);
}

void aarch64::instr_builder::make_st2q_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm) {
	make_ld2_zzprr(b, instr_pos, tag, zt1, zt2, pg, rn, rm, &st2q_zzprr_base);
}

void aarch64::instr_builder::make_st3b_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &st3b_zzzpri_base);
}

void aarch64::instr_builder::make_st3b_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &st3b_zzzprr_base);
}

void aarch64::instr_builder::make_st3h_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &st3h_zzzpri_base);
}

void aarch64::instr_builder::make_st3h_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &st3h_zzzprr_base);
}

void aarch64::instr_builder::make_st3w_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &st3w_zzzpri_base);
}

void aarch64::instr_builder::make_st3w_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &st3w_zzzprr_base);
}

void aarch64::instr_builder::make_st3d_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &st3d_zzzpri_base);
}

void aarch64::instr_builder::make_st3d_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &st3d_zzzprr_base);
}

void aarch64::instr_builder::make_st3q_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm) {
	make_ld3_zzzpri(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, imm, &st3q_zzzpri_base);
}

void aarch64::instr_builder::make_st3q_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm) {
	make_ld3_zzzprr(b, instr_pos, tag, zt1, zt2, zt3, pg, rn, rm, &st3q_zzzprr_base);
}

void aarch64::instr_builder::make_st4b_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &st4b_zzzzpri_base);
}

void aarch64::instr_builder::make_st4b_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &st4b_zzzzprr_base);
}

void aarch64::instr_builder::make_st4h_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &st4h_zzzzpri_base);
}

void aarch64::instr_builder::make_st4h_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &st4h_zzzzprr_base);
}

void aarch64::instr_builder::make_st4w_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &st4w_zzzzpri_base);
}

void aarch64::instr_builder::make_st4w_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &st4w_zzzzprr_base);
}

void aarch64::instr_builder::make_st4d_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &st4d_zzzzpri_base);
}

void aarch64::instr_builder::make_st4d_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &st4d_zzzzprr_base);
}

void aarch64::instr_builder::make_st4q_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm) {
	make_ld4_zzzzpri(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, imm, &st4q_zzzzpri_base);
}

void aarch64::instr_builder::make_st4q_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm) {
	make_ld4_zzzzprr(b, instr_pos, tag, zt1, zt2, zt3, zt4, pg, rn, rm, &st4q_zzzzprr_base);
}

static void make_st1x_zpri(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, int32_t imm,
                           aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, pg, rn };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_st1b_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_st1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &st1b_zpri_base);
}

void aarch64::instr_builder::make_st1h_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_st1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &st1h_zpri_base);
}

void aarch64::instr_builder::make_st1w_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_st1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &st1w_zpri_base);
}

void aarch64::instr_builder::make_st1d_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant zv) {
	make_st1x_zpri(b, instr_pos, rt, pg, rn, imm, zv, &st1d_zpri_base);
}

static void make_st1x_zprr(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, reg rm,
                           aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn, rm);
	std::vector<reg> regs{ rt, pg, rn, rm };
	std::vector<int64_t> literals{ zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_st1b_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_st1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &st1b_zprr_base);
}

void aarch64::instr_builder::make_st1h_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_st1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &st1h_zprr_base);
}

void aarch64::instr_builder::make_st1w_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_st1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &st1w_zprr_base);
}

void aarch64::instr_builder::make_st1d_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant zv) {
	make_st1x_zprr(b, instr_pos, rt, pg, rn, rm, zv, &st1d_zprr_base);
}

static void make_st1x_zprz(block *b, instruction *instr_pos, reg rt, reg pg, reg rn, reg rm,
                           aarch64::shift_amount sh, aarch64::z_type_variant zv, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rt, rm);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::x_regs, rn);
	std::vector<reg> regs{ rt, pg, rn, rm };
	std::vector<int64_t> literals{ sh, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_st1b_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_st1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &st1b_zprz_base);
}

void aarch64::instr_builder::make_st1h_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_st1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &st1h_zprz_base);
}

void aarch64::instr_builder::make_st1w_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_st1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &st1w_zprz_base);
}

void aarch64::instr_builder::make_st1d_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount sh,
                                            z_type_variant zv) {
	make_st1x_zprz(b, instr_pos, rt, pg, rn, rm, sh, zv, &st1d_zprz_base);
}

void aarch64::instr_builder::make_cbnz_ri(reg rt, branch_target *bt) {
	reg_assert_classes_equal_to(x_regs, rt);
	std::vector<reg> regs{ rt };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, &cbnz_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_cbz_ri(reg rt, branch_target *bt) {
	reg_assert_classes_equal_to(x_regs, rt);
	std::vector<reg> regs{ rt };
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, &cbz_ri_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static std::vector<regset> get_pcs_reg_choices_for_args(const std::vector<reg> &regs) {
	using namespace aarch64;
	static std::vector<reg> pcs_xregs = { x0, x1, x2, x3, x4, x5, x6, x7 };
	sloejit_assert(regs.size() <= 8);
	std::vector<regset> ret(regs.size());
	for (unsigned i = 0; i < regs.size(); ++i) {
		// don't bother with non-xregs for now, we don't need it
		reg_assert_classes_equal_to(x_regs, regs[i]);
		ret[i].insert(pcs_xregs[i]);
	}
	return ret;
}

void aarch64::instr_builder::make_bl_i(function *fn, std::vector<reg> regs, std::vector<bool> input_mask,
                                       std::vector<bool> output_mask) {
	// TODO: we should support non-false output masks
	sloejit_assert(regs.size() <= 8);
	sloejit_assert(input_mask.size() == regs.size());
	sloejit_assert(output_mask.size() == regs.size());
	sloejit_assert(std::all_of(input_mask.begin(), input_mask.end(), [](bool x) { return x; }));
	sloejit_assert(std::all_of(output_mask.begin(), output_mask.end(), [](bool x) { return !x; }));
	sloejit_assert(std::all_of(regs.begin(), regs.end(),
	                   [](reg x) { return x.space_id == x_space && x.active_mask == x_regs; }));
	auto reg_choices = get_pcs_reg_choices_for_args(regs);
	std::vector<uint8_t> input_active_mask(input_mask.size(), x_regs);
	std::vector<uint8_t> output_active_mask(output_mask.size(), 0);
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ fn };
	make_instr(*b, instr_pos, &bl_i_base, tag, std::move(regs), std::move(input_mask), std::move(output_mask),
	           std::move(input_active_mask), std::move(output_active_mask), std::move(reg_choices),
	           std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_blr_r(reg rn, std::vector<reg> regs, std::vector<bool> input_mask,
                                        std::vector<bool> output_mask) {
	// TODO: we should support non-false output masks
	sloejit_assert(regs.size() <= 8);
	sloejit_assert(input_mask.size() == regs.size());
	sloejit_assert(output_mask.size() == regs.size());
	sloejit_assert(std::all_of(input_mask.begin(), input_mask.end(), [](bool x) { return x; }));
	sloejit_assert(std::all_of(output_mask.begin(), output_mask.end(), [](bool x) { return !x; }));
	sloejit_assert(std::all_of(regs.begin(), regs.end(),
	                   [](reg x) { return x.space_id == x_space && x.active_mask == x_regs; }));
	reg_assert_classes_equal_to(x_regs, rn);
	auto reg_choices = get_pcs_reg_choices_for_args(regs);
	regs.insert(regs.begin(), rn);
	reg_choices.insert(reg_choices.begin(), aarch64_regs_for_space(x_space).as_regset());
	std::vector<uint8_t> input_active_mask(input_mask.size(), x_regs);
	std::vector<uint8_t> output_active_mask(output_mask.size(), 0);
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &blr_r_base, tag, std::move(regs), std::move(input_mask), std::move(output_mask),
	           std::move(input_active_mask), std::move(output_active_mask), std::move(reg_choices),
	           std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_b_i(branch_target *bt) {
	std::vector<reg> regs;
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, &b_i_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

template <int cond>
static void make_b_cond_b(block *b, instruction *instr_pos, branch_target *bt, const instr_base *base) {
	std::vector<reg> regs;
	std::vector<int64_t> literals{ cond };
	std::vector<branch_target *> targets{ bt };
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

template <int cond>
static void make_b_cond_i(block *b, instruction *instr_pos, int64_t imm, const instr_base *base) {
	std::vector<reg> regs;
	std::vector<int64_t> literals{ cond, imm };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_b_eq_i(int64_t imm) {
	make_b_cond_i<0b0000>(b, instr_pos, imm, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_eq_b(branch_target *bt) {
	make_b_cond_b<0b0000>(b, instr_pos, bt, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_ne_i(int64_t imm) {
	make_b_cond_i<0b0001>(b, instr_pos, imm, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_ne_b(branch_target *bt) {
	make_b_cond_b<0b0001>(b, instr_pos, bt, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_le_i(int64_t imm) {
	make_b_cond_i<0b1101>(b, instr_pos, imm, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_le_b(branch_target *bt) {
	make_b_cond_b<0b1101>(b, instr_pos, bt, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_lt_i(int64_t imm) {
	make_b_cond_i<0b1011>(b, instr_pos, imm, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_lt_b(branch_target *bt) {
	make_b_cond_b<0b1011>(b, instr_pos, bt, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_ge_i(int64_t imm) {
	make_b_cond_i<0b1010>(b, instr_pos, imm, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_ge_b(branch_target *bt) {
	make_b_cond_b<0b1010>(b, instr_pos, bt, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_gt_i(int64_t imm) {
	make_b_cond_i<0b1100>(b, instr_pos, imm, &b_cond_i_base);
}

void aarch64::instr_builder::make_b_gt_b(branch_target *bt) {
	make_b_cond_b<0b1100>(b, instr_pos, bt, &b_cond_i_base);
}

void aarch64::instr_builder::make_ret() {
	std::vector<reg> regs;
	std::vector<int64_t> literals;
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &ret__base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_x_mov_rr(reg rd, reg rm) {
	if (rd == sp || rm == sp) {
		make_x_add_rri(rd, rm, 0);
	}
	else {
		make_x_orr_rrr(rd, xzr, rm);
	}
}

void aarch64::instr_builder::make_x_cmp_rr(reg rn, reg rm) {
	make_x_subs_rrr(xzr, rn, rm);
}

void aarch64::instr_builder::make_x_cmp_ri(reg rn, int imm) {
	make_x_subs_rri(xzr, rn, imm);
}

void aarch64::instr_builder::make_smstart() {
	make_smstart_i(aarch64::smopt_both);
}

void aarch64::instr_builder::make_smstart_i(smopt opt) {
	std::vector<reg> regs;
	std::vector<int64_t> literals{opt};
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smstart_i_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smstop() {
	make_smstop_i(aarch64::smopt_both);
}

void aarch64::instr_builder::make_smstop_i(smopt opt) {
	std::vector<reg> regs;
	std::vector<int64_t> literals{opt};
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smstop_i_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlal_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlal_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlal2_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlal2_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlsl_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlsl_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlsl2_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlsl2_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_smull_qq(reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smull_qqq(rd, rn, rm, to, from);
	return rd;
}

void aarch64::instr_builder::make_smull_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smull_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_smull2_qq(reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smull2_qqq(rd, rn, rm, to, from);
	return rd;
}

void aarch64::instr_builder::make_smull2_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smull2_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_smlasl_op_qqql(block *b, instruction *instr_pos, reg rd, reg rn, reg rm, int lane,
                                aarch64::q_type_variant to, aarch64::q_type_variant from, const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::q_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ lane, to, from };
	std::vector<branch_target *> targets;
	if (from == aarch64::q_type_variant::qv_4h || from == aarch64::q_type_variant::qv_8h) {
		// Indexed accumulation requires that the second input register be restricted
		// to the range v0 - v15 for half precision
		std::vector<bool> input_mask;
		std::vector<bool> output_mask;
		std::vector<uint8_t> input_active_mask;
		std::vector<uint8_t> output_active_mask;
		auto all_vregs = aarch64_regs_for_space(aarch64::v_space).as_regset();
		auto low_vregs = aarch64_regs_for_space_low_half(aarch64::v_space).as_regset();
		std::vector<regset> reg_choices = { all_vregs, all_vregs, low_vregs };
		make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(input_mask), std::move(output_mask), std::move(input_active_mask), std::move(output_active_mask),
		    std::move(reg_choices), std::move(literals), std::move(targets));
	}
	else {
		make_instr(*b, instr_pos, base, std::nullopt, std::move(regs), std::move(literals), std::move(targets));
	}
}

void aarch64::instr_builder::make_smlal_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	make_smlasl_op_qqql(b, instr_pos, rd, rn, rm, lane, to, from, &smlal_qqql_base);
}

void aarch64::instr_builder::make_smlal2_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	make_smlasl_op_qqql(b, instr_pos, rd, rn, rm, lane, to, from, &smlal2_qqql_base);
}

void aarch64::instr_builder::make_smlsl_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	make_smlasl_op_qqql(b, instr_pos, rd, rn, rm, lane, to, from, &smlsl_qqql_base);
}

void aarch64::instr_builder::make_smlsl2_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	make_smlasl_op_qqql(b, instr_pos, rd, rn, rm, lane, to, from, &smlsl2_qqql_base);
}

reg aarch64::instr_builder::make_smull_qql(reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smull_qqql(rd, rn, rm, lane, to, from);
	return rd;
}

void aarch64::instr_builder::make_smull_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	make_smlasl_op_qqql(b, instr_pos, rd, rn, rm, lane, to, from, &smull_qqql_base);
}

reg aarch64::instr_builder::make_smull2_qql(reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smull2_qqql(rd, rn, rm, lane, to, from);
	return rd;
}

void aarch64::instr_builder::make_smull2_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from) {
	make_smlasl_op_qqql(b, instr_pos, rd, rn, rm, lane, to, from, &smull2_qqql_base);
}

reg aarch64::instr_builder::make_sqneg_pz(reg pg, reg rn, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	reg_assert_classes_equal_to(aarch64::z_regs, rn);
	reg rd = b->fresh_vreg(aarch64::v_space, aarch64::z_regs);
	make_sqneg_zpz(rd, pg, rn, zv);
	return rd;
}

void aarch64::instr_builder::make_sqneg_zpz(reg rd, reg pg, reg rn, z_type_variant zv) {
	make_threearg_zpz(b, instr_pos, rd, pg, rn, zv, &sqneg_zpz_base);
}

reg aarch64::instr_builder::make_sqneg_q(reg rn, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqneg_qq(rd, rn, qv);
	return rd;
}

void aarch64::instr_builder::make_sqneg_qq(reg rd, reg rn, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqneg_qq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sqadd_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &sqadd_zzz_base);
}

void aarch64::instr_builder::make_sqadd_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &sqadd_zzz_base);
}

reg aarch64::instr_builder::make_sqadd_qq(reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqadd_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_sqadd_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqadd_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sqsub_zz(reg rn, reg rm, z_type_variant zv) {
	return make_threearg_zz(b, instr_pos, rn, rm, zv, &sqsub_zzz_base);
}

void aarch64::instr_builder::make_sqsub_zzz(reg rd, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rd, rn, rm, zv, &sqsub_zzz_base);
}

reg aarch64::instr_builder::make_sqsub_qq(reg rn, reg rm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqsub_qqq(rd, rn, rm, qv);
	return rd;
}

void aarch64::instr_builder::make_sqsub_qqq(reg rd, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqsub_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlalb_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlalb_zzz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlalt_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlalt_zzz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlslb_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlslb_zzz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_smlslt_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smlslt_zzz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_smullb_zz(reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smullb_zzz(rd, rn, rm, to, from);
	return rd;
}

void aarch64::instr_builder::make_smullb_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smullb_zzz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_smullt_zz(reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smullt_zzz(rd, rn, rm, to, from);
	return rd;
}

void aarch64::instr_builder::make_smullt_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &smullt_zzz_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

static void make_smlasl_op_zzzl(block *b, instruction *instr_pos, const std::optional<std::string> &tag, reg rd, reg rn,
                                reg rm, unsigned lane, aarch64::z_type_variant to, aarch64::z_type_variant from,
                                const instr_base *base) {
	reg_assert_classes_equal_to(aarch64::z_regs, rd, rn, rm);
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ lane, to, from };
	std::vector<branch_target *> targets;
	if (to == aarch64::zv_s && from == aarch64::zv_h) {
		std::vector<bool> input_mask;
		std::vector<bool> output_mask;
		std::vector<uint8_t> input_active_mask;
		std::vector<uint8_t> output_active_mask;
		auto all_zregs = aarch64_regs_for_space(aarch64::v_space).as_regset();
		auto low_zregs = aarch64_regs_for_space_low_quarter(aarch64::v_space).as_regset();
		std::vector<regset> reg_choices = { all_zregs, all_zregs, low_zregs };
		make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(input_mask), std::move(output_mask),
		    std::move(input_active_mask), std::move(output_active_mask), std::move(reg_choices), std::move(literals),
		    std::move(targets));
	}
	else if (to == aarch64::zv_d && from == aarch64::zv_s) {
		std::vector<bool> input_mask;
		std::vector<bool> output_mask;
		std::vector<uint8_t> input_active_mask;
		std::vector<uint8_t> output_active_mask;
		auto all_zregs = aarch64_regs_for_space(aarch64::v_space).as_regset();
		auto low_zregs = aarch64_regs_for_space_low_half(aarch64::v_space).as_regset();
		std::vector<regset> reg_choices = { all_zregs, all_zregs, low_zregs };
		make_instr(*b, instr_pos, base, tag, std::move(regs), std::move(input_mask), std::move(output_mask),
		    std::move(input_active_mask), std::move(output_active_mask), std::move(reg_choices), std::move(literals),
		    std::move(targets));
	}
	else {
		sloejit_assert(false);
	}
}

void aarch64::instr_builder::make_smlalb_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	make_smlasl_op_zzzl(b, instr_pos, tag, rd, rn, rm, lane, to, from, &smlalb_zzzl_base);
}

void aarch64::instr_builder::make_smlalt_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	make_smlasl_op_zzzl(b, instr_pos, tag, rd, rn, rm, lane, to, from, &smlalt_zzzl_base);
}

void aarch64::instr_builder::make_smlslb_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	make_smlasl_op_zzzl(b, instr_pos, tag, rd, rn, rm, lane, to, from, &smlslb_zzzl_base);
}

void aarch64::instr_builder::make_smlslt_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	make_smlasl_op_zzzl(b, instr_pos, tag, rd, rn, rm, lane, to, from, &smlslt_zzzl_base);
}

reg aarch64::instr_builder::make_smullb_zzl(reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smullb_zzzl(rd, rn, rm, lane, to, from);
	return rd;
}

void aarch64::instr_builder::make_smullb_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	make_smlasl_op_zzzl(b, instr_pos, tag, rd, rn, rm, lane, to, from, &smullb_zzzl_base);
}

reg aarch64::instr_builder::make_smullt_zzl(reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_smullt_zzzl(rd, rn, rm, lane, to, from);
	return rd;
}

void aarch64::instr_builder::make_smullt_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from) {
	make_smlasl_op_zzzl(b, instr_pos, tag, rd, rn, rm, lane, to, from, &smullt_zzzl_base);
}

reg aarch64::instr_builder::make_sqrdmulh_zzl(reg rn, reg rm, unsigned lane, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqrdmulh_zzzl(rd, rn, rm, lane, zv);
	return rd;
}

void aarch64::instr_builder::make_sqrdmulh_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant zv) {
	std::vector<reg> regs{ rd, rn, rm };
	std::vector<int64_t> literals{ lane, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdmulh_zzzl_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrdmlah_zzzl(reg rda, reg rn, reg rm, unsigned lane, z_type_variant zv) {
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ lane, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdmlah_zzzl_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrdmlsh_zzzl(reg rda, reg rn, reg rm, unsigned lane, z_type_variant zv) {
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ lane, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdmlsh_zzzl_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sqrdmulh_qql(reg rn, reg rm, unsigned lane, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn, rm);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqrdmulh_qqql(rd, rn, rm, lane, qv);
	return rd;
}

void aarch64::instr_builder::make_sqrdmulh_qqql(reg rda, reg rn, reg rm, unsigned lane, q_type_variant qv) {
	make_f_acc_op_qqql(b, instr_pos, rda, rn, rm, lane, qv, &sqrdmulh_qqql_base);
}

void aarch64::instr_builder::make_sqrdmlah_qqql(reg rda, reg rn, reg rm, unsigned lane, q_type_variant qv) {
	make_f_acc_op_qqql(b, instr_pos, rda, rn, rm, lane, qv, &sqrdmlah_qqql_base);
}

void aarch64::instr_builder::make_sqrdmlsh_qqql(reg rda, reg rn, reg rm, unsigned lane, q_type_variant qv) {
	make_f_acc_op_qqql(b, instr_pos, rda, rn, rm, lane, qv, &sqrdmlsh_qqql_base);
}

void aarch64::instr_builder::make_sqrdmulh_qqq(reg rda, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdmulh_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrdmlah_qqq(reg rda, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdmlah_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrdmlsh_qqq(reg rda, reg rn, reg rm, q_type_variant qv) {
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdmlsh_qqq_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrdmulh_zzz(reg rda, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rda, rn, rm, zv, &sqrdmulh_zzz_base);
}

void aarch64::instr_builder::make_sqrdmlah_zzz(reg rda, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rda, rn, rm, zv, &sqrdmlah_zzz_base);
}

void aarch64::instr_builder::make_sqrdmlsh_zzz(reg rda, reg rn, reg rm, z_type_variant zv) {
	make_threearg_zzz(b, instr_pos, rda, rn, rm, zv, &sqrdmlsh_zzz_base);
}

void aarch64::instr_builder::make_sqrdcmlah_zzzi(reg rda, reg rn, reg rm, int rot, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rda, rn, rm);
	std::vector<reg> regs{ rda, rn, rm };
	std::vector<int64_t> literals{ rot, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrdcmlah_zzzi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrshrnb_zzi(reg rdn, reg rn, unsigned imm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn, rn);
	std::vector<reg> regs{ rdn, rn };
	std::vector<int64_t> literals{ imm, to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrshrnb_zzi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

void aarch64::instr_builder::make_sqrshrnt_zzi(reg rdn, reg rn, unsigned imm, z_type_variant to, z_type_variant from) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn, rn);
	std::vector<reg> regs{ rdn, rn };
	std::vector<int64_t> literals{ imm, to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrshrnt_zzi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sqrshrn_qi(reg rn, unsigned imm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqrshrn_qqi(rd, rn, imm, to, from);
	return rd;
}

void aarch64::instr_builder::make_sqrshrn_qqi(reg rd, reg rn, unsigned imm, q_type_variant to, q_type_variant from) {
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm, to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrshrn_qqi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_sqrshrn2_qi(reg rn, unsigned imm, q_type_variant to, q_type_variant from) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_sqrshrn2_qqi(rd, rn, imm, to, from);
	return rd;
}

void aarch64::instr_builder::make_sqrshrn2_qqi(reg rd, reg rn, unsigned imm, q_type_variant to, q_type_variant from) {
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm, to, from };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &sqrshrn2_qqi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}


void aarch64::instr_builder::make_srshr_zpi(reg rdn, reg pg, unsigned imm, z_type_variant zv) {
	reg_assert_classes_equal_to(aarch64::z_regs, rdn);
	reg_assert_classes_equal_to(aarch64::p_regs, pg);
	std::vector<reg> regs{ rdn, pg };
	std::vector<int64_t> literals{ imm, zv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &srshr_zpi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}

reg aarch64::instr_builder::make_srshr_qi(reg rn, unsigned imm, q_type_variant qv) {
	reg_assert_classes_equal_to(aarch64::q_regs, rn);
	reg rd = b->fresh_vreg(v_space, rn.active_mask);
	make_srshr_qqi(rd, rn, imm, qv);
	return rd;
}

void aarch64::instr_builder::make_srshr_qqi(reg rd, reg rn, unsigned imm, q_type_variant qv) {
	std::vector<reg> regs{ rd, rn };
	std::vector<int64_t> literals{ imm, qv };
	std::vector<branch_target *> targets;
	make_instr(*b, instr_pos, &srshr_qqi_base, tag, std::move(regs), std::move(literals), std::move(targets));
}


static regset get_pcs_preserve() {
	// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
	// (section 5.1: Machine Registers)
	// note: x29 is technically also preserved, but we treat this specially
	//       (see {adjust,erase}_special_regs), so ignore that in this list.
	// x23, x24, x28 do not exist on Arm64EC and so we remove them here
	// See https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
	using namespace aarch64;
	static regset ret = {
#if !defined(PLFFT_ENABLE_ARM64EC)
		x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, d8, d9, d10, d11, d12, d13, d14, d15,
#else
		x19, x20, x21, x22,           x25, x26, x27,      d8, d9, d10, d11, d12, d13, d14, d15,
#endif
	};
	return ret;
}

static regset_one_space get_x_regs() {
	// x23, x24, x28 do not exist on Arm64EC and so we remove them here
	// See https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
	// x18 may not be used on mac, android, or windows. Remove it for all builds to maintain kernel generality
	using namespace aarch64;
	static regset_one_space ret = {
#if !defined(PLFFT_ENABLE_ARM64EC)
		xzr, sp,  x0,  x1,  x2,  x3,  x4,  x5,  x6,  x7,  x8,  x9,  x10, x11, x12, x13, x14,
		x15, x16, x17,      x19, x20, x21, x22, x23, x24, x25 ,x26, x27, x28, x29, x30,
#else
		xzr, sp,  x0,  x1,  x2,  x3,  x4,  x5,  x6,  x7,  x8,  x9,  x10, x11, x12,
		x15, x16, x17,      x19, x20, x21, x22,           x25, x26, x27,      x29, x30,
#endif
	};
	return ret;
}

static regset_one_space get_za_regs() {
	using namespace aarch64;
	static regset_one_space ret = { za };
	return ret;
}

static regset_one_space get_fp_regs(uint64_t space_id, uint64_t base, uint64_t count, uint8_t active_mask) {
	regset_one_space ret;
	for (uint64_t i = 0; i < count; ++i) {
		ret.insert({ space_id, base + i, active_mask });
	}
	return ret;
}

static const regset_one_space &aarch64_regs_for_space(uint64_t space_id) {
	static regset_one_space x_regs = get_x_regs();
	static regset_one_space v_regs = get_fp_regs(aarch64::v_space, 1, 32, aarch64::z_regs);
	static regset_one_space p_regs = get_fp_regs(aarch64::p_space, 1, 8, aarch64::p_regs);
	static regset_one_space za_regs = get_za_regs();
	switch ((aarch64::preg_spaces) space_id) {
	case aarch64::x_space: return x_regs;
	case aarch64::v_space: return v_regs;
	case aarch64::p_space: return p_regs;
	case aarch64::za_space: return za_regs;
	}
	sloejit_assert(false);
	return x_regs;
}

static const regset_one_space &aarch64_regs_for_space_low_half(uint64_t space_id) {
	static regset_one_space vr = get_fp_regs(aarch64::v_space, 1, 16, aarch64::z_regs);
	switch((aarch64::preg_spaces) space_id) {
	case aarch64::x_space: break;
	case aarch64::v_space: return vr;
	case aarch64::p_space: break;
	case aarch64::za_space: break;
	}
	sloejit_assert(false);
	return vr;
}

static const regset_one_space &aarch64_regs_for_space_low_quarter(uint64_t space_id) {
	static regset_one_space vr = get_fp_regs(aarch64::v_space, 1, 8, aarch64::z_regs);
	switch((aarch64::preg_spaces) space_id) {
	case aarch64::x_space: break;
	case aarch64::v_space: return vr;
	case aarch64::p_space: break;
	case aarch64::za_space: break;
	}
	sloejit_assert(false);
	return vr;
}

static bool aarch64_instr_is_redundant(const instruction *i) {
	switch ((aarch64::opcode) i->base->opcode) {
	case aarch64::opcode::orr_rrr: {
		auto [dst, src0, src1] = i->get_regs<0, 1, 2>();
		return dst == aarch64::xzr ||
		       (dst == src0 && src1 == aarch64::xzr) ||
		       (dst == src1 && src0 == aarch64::xzr);
	}
	case aarch64::opcode::ext_qqqi: {
		auto [dst, src0, src1] = i->get_regs<0, 1, 2>();
		return dst == src0 && i->get_literal(0) == 0;
	}
	default: return false;
	}
}

static std::optional<int> stack_spill_instr_get_ofs(const instruction *i) {
	switch ((aarch64::opcode) i->base->opcode) {
	case aarch64::opcode::str_b_bri:
	case aarch64::opcode::str_h_hri:
	case aarch64::opcode::str_s_sri:
	case aarch64::opcode::str_d_dri:
	case aarch64::opcode::str_q_qri:
	case aarch64::opcode::str_zri:
	case aarch64::opcode::str_x_rri:
		if (i->get_reg(1) == aarch64::x29 || i->get_reg(1) == aarch64::sp) {
			return i->get_literal(0);
		}
		return std::nullopt;
	default: return std::nullopt;
	}
}

static std::optional<int> stack_reload_instr_get_ofs(const instruction *i) {
	switch ((aarch64::opcode) i->base->opcode) {
	case aarch64::opcode::ldr_b_bri:
	case aarch64::opcode::ldr_h_hri:
	case aarch64::opcode::ldr_s_sri:
	case aarch64::opcode::ldr_d_dri:
	case aarch64::opcode::ldr_q_qri:
	case aarch64::opcode::ldr_zri:
	case aarch64::opcode::ldr_x_rri:
		if (i->get_reg(1) == aarch64::x29 || i->get_reg(1) == aarch64::sp) {
			return i->get_literal(0);
		}
		return std::nullopt;
	default: return std::nullopt;
	}
}

static bool is_spill_fill_instr(const instruction *i) {
	return stack_spill_instr_get_ofs(i) || stack_reload_instr_get_ofs(i);
}

struct elem_width {
	bool sve;
	int bytes;

	inline bool operator==(elem_width other) const {
		return sve == other.sve && bytes == other.bytes;
	}

	inline bool operator!=(elem_width other) const {
		return !(*this == other);
	}
};

static elem_width elem_width_max(elem_width a, elem_width b) {
	return { a.sve || b.sve, std::max(a.bytes, b.bytes) };
}

static elem_width get_spill_elem_width(const instruction *i) {
	switch ((aarch64::opcode) i->base->opcode) {
	case aarch64::opcode::ldr_b_bri:
	case aarch64::opcode::str_b_bri: return { false, 1 };
	case aarch64::opcode::ldr_h_hri:
	case aarch64::opcode::str_h_hri: return { false, 2 };
	case aarch64::opcode::ldr_s_sri:
	case aarch64::opcode::str_s_sri: return { false, 4 };
	case aarch64::opcode::ldr_d_dri:
	case aarch64::opcode::str_d_dri: return { false, 8 };
	case aarch64::opcode::ldr_q_qri:
	case aarch64::opcode::str_q_qri: return { false, 16 };
	case aarch64::opcode::ldr_x_rri:
	case aarch64::opcode::str_x_rri: return { false, 8 };
	case aarch64::opcode::ldr_zri:
	case aarch64::opcode::str_zri: return { true, 16 };
	default: sloejit_assert(false && "unhandled spill instruction");
	}
	return {false, 0};
}

struct stack_slot {
	elem_width width;
	std::map<block *, live_range> range;
};

struct aarch64_traits : public sloejit::arch_traits {
	aarch64_traits() : arch_traits(aarch64::max_preg_num, get_pcs_preserve()) {
	}

	const regset_one_space &regs_for_space(uint64_t space_id) const override {
		return aarch64_regs_for_space(space_id);
	}

	const regset_one_space &regs_for_space_low_half(uint64_t space_id) const override {
		return aarch64_regs_for_space_low_half(space_id);
	}

	void adjust_special_regs(regset &live) const override {
		// x13, x14, x23, x24, x28 do not exist on Arm64EC and so we remove them here
		// See https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
#if defined(PLFFT_ENABLE_ARM64EC)
		live.insert(aarch64::x13);
		live.insert(aarch64::x14);
		live.insert(aarch64::x23);
		live.insert(aarch64::x24);
		live.insert(aarch64::x28);
#endif
		live.insert(aarch64::x29);
		live.insert(aarch64::x30);
		live.erase(aarch64::x_space, aarch64::xzr.id);
		live.erase(aarch64::x_space, aarch64::sp.id);
		live.erase(aarch64::x_space, aarch64::pc.id);
	}

	void erase_special_regs(regset_one_space &live) const override {
		// x13, x14, x23, x24, x28 do not exist on Arm64EC and so we remove them here
		// See https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
#if defined(PLFFT_ENABLE_ARM64EC)
		live.erase(aarch64::x13);
		live.erase(aarch64::x14);
		live.erase(aarch64::x23);
		live.erase(aarch64::x24);
		live.erase(aarch64::x28);
#endif
		live.erase(aarch64::x29);
		live.erase(aarch64::x30);
		live.erase(aarch64::xzr);
		live.erase(aarch64::sp);
		live.erase(aarch64::pc);
	}

	instruction *emit_spill(block *, instruction *instr, reg r) const override {
		sloejit_assert(r.active_mask > 0);
		aarch64::instr_builder ib{ instr->parent, instr->instr_next }; // insert after instr
		ib.tag = "stack spill";
		switch ((aarch64::preg_spaces) r.space_id) {
		case aarch64::x_space: ib.make_x_str_rri(r, aarch64::x29, 0); return instr->instr_next;
		case aarch64::v_space: break;
		case aarch64::p_space: sloejit_assert(false); __builtin_unreachable();
		case aarch64::za_space: sloejit_assert(false); __builtin_unreachable();
		}
		sloejit_assert(r.space_id == aarch64::v_space);
		switch ((aarch64::preg_classes) r.active_mask) {
		case aarch64::b_regs: ib.make_b_str_rri(r, aarch64::x29, 0); break;
		case aarch64::h_regs: ib.make_h_str_rri(r, aarch64::x29, 0); break;
		case aarch64::s_regs: ib.make_s_str_rri(r, aarch64::x29, 0); break;
		case aarch64::d_regs: ib.make_d_str_rri(r, aarch64::x29, 0); break;
		case aarch64::q_regs: ib.make_q_str_rri(r, aarch64::x29, 0); break;
		case aarch64::z_regs: ib.make_str_zri(r, aarch64::x29, 0); break;
		default: sloejit_assert(false);
		}
		return instr->instr_next;
	}

	instruction *emit_reload(block *, instruction *instr, reg r) const override {
		sloejit_assert(r.active_mask > 0);
		sloejit_assert(r.space_id != aarch64::p_space && r.space_id != aarch64::za_space);
		aarch64::instr_builder ib{ instr->parent, instr }; // insert before instr
		ib.tag = "stack reload";
		switch ((aarch64::preg_spaces) r.space_id) {
		case aarch64::x_space: ib.make_x_ldr_rri(r, aarch64::x29, 0); return instr->instr_prev;
		case aarch64::v_space: break;
		case aarch64::p_space: sloejit_assert(false); __builtin_unreachable();
		case aarch64::za_space: sloejit_assert(false); __builtin_unreachable();
		}
		sloejit_assert(r.space_id == aarch64::v_space);
		switch ((aarch64::preg_classes) r.active_mask) {
		case aarch64::b_regs: ib.make_b_ldr_rri(r, aarch64::x29, 0); break;
		case aarch64::h_regs: ib.make_h_ldr_rri(r, aarch64::x29, 0); break;
		case aarch64::s_regs: ib.make_s_ldr_rri(r, aarch64::x29, 0); break;
		case aarch64::d_regs: ib.make_d_ldr_rri(r, aarch64::x29, 0); break;
		case aarch64::q_regs: ib.make_q_ldr_rri(r, aarch64::x29, 0); break;
		case aarch64::z_regs: ib.make_ldr_zri(r, aarch64::x29, 0); break;
		default: sloejit_assert(false);
		}
		return instr->instr_prev;
	}

	instruction *emit_pcs_spill(block *, instruction *instr, reg r) const override {
		aarch64::instr_builder ib{ instr->parent, instr }; // insert before instr
		switch (r.space_id) {
		case aarch64::x_space: {
			r = aarch64::reg_reinterpret_with_class(r, aarch64::x_regs);
			ib.make_x_str_rri(r, aarch64::x29, 0);
			break;
		}
		case aarch64::v_space: {
			r = aarch64::reg_reinterpret_with_class(r, aarch64::d_regs);
			ib.make_d_str_rri(r, aarch64::x29, 0);
			break;
		}
		case aarch64::p_space: sloejit_assert(false);
		}
		return instr->instr_prev;
	}

	instruction *emit_pcs_reload(block *, instruction *instr, reg r) const override {
		aarch64::instr_builder ib{ instr->parent, instr }; // insert before instr
		switch (r.space_id) {
		case aarch64::x_space: {
			r = aarch64::reg_reinterpret_with_class(r, aarch64::x_regs);
			ib.make_x_ldr_rri(r, aarch64::x29, 0);
			break;
		}
		case aarch64::v_space: {
			r = aarch64::reg_reinterpret_with_class(r, aarch64::d_regs);
			ib.make_d_ldr_rri(r, aarch64::x29, 0);
			break;
		}
		case aarch64::p_space: sloejit_assert(false);
		}
		return instr->instr_prev;
	}

	std::map<reg, regset> get_pcs_reg_choices(const std::map<reg, reg> &pcs_vreg_map,
	                                          const std::vector<reg> &inputs,
	                                          const std::vector<reg> &outputs) const override {
		using namespace aarch64;
		// TODO: we should allow a non-zero number of return values
		std::map<reg, regset> ret;
		auto pcs_saved_regs = get_pcs_preserve();
		unsigned npcs_regs = pcs_saved_regs.size();
		// check outputs match pcs callee-save, set choices to pcs preg.
		sloejit_assert(outputs.size() == npcs_regs);
		for (unsigned i = 0; i < npcs_regs; ++i) {
			reg out_vreg = outputs[i];
			reg out_preg = pcs_vreg_map.at(out_vreg);
			sloejit_assert(pcs_saved_regs.count(out_preg));
			regset out_pregs{ out_preg };
			if (ret.count(out_vreg)) {
				ret.at(out_vreg).intersect(out_pregs);
			}
			else {
				ret.emplace(out_vreg, out_pregs);
			}
		}
		// check first n inputs match pcs callee-save, then pcs parameter order
		sloejit_assert(inputs.size() >= npcs_regs);
		for (unsigned i = 0; i < npcs_regs; ++i) {
			reg in_vreg = inputs[i];
			reg in_preg = pcs_vreg_map.at(in_vreg);
			sloejit_assert(pcs_saved_regs.count(in_preg));
			regset in_pregs{ in_preg };
			if (ret.count(in_vreg)) {
				ret.at(in_vreg).intersect(in_pregs);
			}
			else {
				ret.emplace(in_vreg, in_pregs);
			}
		}
		sloejit_assert(inputs.size() - npcs_regs <= 8);
		for (unsigned i = npcs_regs; i < inputs.size(); ++i) {
			static std::vector<reg> pcs_xregs = { x0, x1, x2, x3, x4, x5, x6, x7 };
			reg in_vreg = inputs[i];
			// don't bother with non-xregs for now, we don't need it
			reg_assert_classes_equal_to(x_regs, in_vreg);
			regset in_pregs{ pcs_xregs[i - npcs_regs] };
			if (ret.count(in_vreg)) {
				ret.at(in_vreg).intersect(in_pregs);
			}
			else {
				ret.emplace(in_vreg, in_pregs);
			}
		}
		return ret;
	}

	void finalize_spills(function *fn, const sloejit::stack_frame_info *frame_info,
	                     std::map<reg, std::vector<instruction *>> &spill_details,
	                     const sloejit::live_matrix &spill_ranges, block *prologue) const override {
		sloejit_assert(fn);

		// the input function may have requested that we modify non-spill
		// stack accesses to accommodate spills, in which case we need to know
		// how big they were expecting the stack to be above and below the
		// spill frame.
		int child_call_arg_stack_size = frame_info ? frame_info->child_call_arg_stack_size : 0;
		int parent_call_arg_stack_size = frame_info ? frame_info->parent_call_arg_stack_size : 0;
		sloejit_assert(child_call_arg_stack_size >= 0);
		sloejit_assert(child_call_arg_stack_size % 16 == 0);

		// If there are no spills and no stack arguments, and the function
		// options don't explicitly request it, then don't create a stack frame
		if (child_call_arg_stack_size == 0 && parent_call_arg_stack_size == 0 &&
		    spill_details.empty() && !fn->opts.keep_frame_pointer) {
			return;
		}

		// TODO: most of this code will eventually disappear into target-
		//       specific hooks, called from (rather than pasted) here.
		// make a prologue block (reg_spill may have already created this block
		// for PCS spills).
		if (!prologue) {
			prologue = fn->make_block("_prologue", 0);
			aarch64::instr_builder prologue_ib{ prologue };
			prologue_ib.make_b_i(&*fn->blocks[1]);
		} else {
			sloejit_assert(!fn->blocks.empty());
			sloejit_assert(prologue == &*fn->blocks[0]);
		}

		// stack layout (sspill are scalar spills, vspill are SVE vector spills):
		//        |  argn  |
		// sp0 -> |  arg0  | (sp on input)
		//        | sspill |
		//        | sspill |
		//        |  x30   |
		// x29 -> |  x29   | (x29 in body, k+16 below sp0)
		//        | vspill |
		//        | vspill |
		//        |  argn  |
		// sp1 -> |  arg0  | (sp in body, m*vl + n below x29)

		// figure out total stack spill size, build reg->ofs mapping
		// ofs starts at 16 to accommodate x29/x30 stored at ofs 0/8.
		int scalar_ofs = 16;
		int vector_ofs = 0; // mul vl

		// 16 * SVL means 1024 bytes for SVL == 512, meaning vector spills are safe from LSRT hazard
		constexpr int sme_vector_spill_pad_vls = 16;
		if (fn->opts.sme != function_options_t::sme_usage::none) {
			vector_ofs -= sme_vector_spill_pad_vls;
		}

		std::set<instruction *> all_spill_instrs;

		// keep track of what slots are occupied where in the function, so we
		// can reuse slots for non-overlapping spills.
		std::vector<std::pair<int, stack_slot>> slots;

		for (auto &p : spill_details) {
			all_spill_instrs.insert(p.second.begin(), p.second.end());
			elem_width sz{ false, 0 };
			// figure out how wide a slot we need, ideally these would be
			// identical but that's not always the case (at least for now).
			for (instruction *i : p.second) {
				sz = elem_width_max(sz, get_spill_elem_width(i));
				sloejit_assert(i->get_reg(1) == aarch64::x29);
				sloejit_assert(i->literals.at(0) == 0);
			}
			sloejit_assert(sz.bytes > 0);
			std::map<int, std::pair<block *, const live_range *>> current_range = spill_ranges.at(p.first);

			// try and find a stack slot that we can reuse (because it is only
			// storing a value for a non-overlapping live range). This is a
			// matter of just checking that the slot is the same size as what
			// we want and the existing use of that slot does not overlap.
			auto it = std::find_if(slots.begin(), slots.end(), [&](auto &p) {
				if (sz != p.second.width) {
					return false;
				}
				for (const auto &elem1 : current_range) {
					auto [b1, lr1] = elem1.second;
					for (auto [b2, lr2] : p.second.range) {
						if (b1 == b2 && lr1->overlaps(lr2)) {
							return false;
						}
					}
				}
				return true;
			});
			int current_ofs;
			if (it != slots.end()) {
				current_ofs = it->first;
				auto &lr1 = it->second.range;
				for (const auto &elem : current_range) {
					auto [b, lr2] = elem.second;
					lr1[b].insert(*lr2);
				}
			}
			else {
				if (sz.sve) {
					// no need to worry about alignment here, since vectors are
					// always at least a multiple of 16 bytes (note: this isn't
					// true for predicates, but we don't spill those yet anyway!)
					current_ofs = --vector_ofs;
				}
				else {
					// round scalar ofs up to a multiple of sz for alignment
					if (scalar_ofs % sz.bytes > 0) {
						scalar_ofs += sz.bytes - (scalar_ofs % sz.bytes);
					}
					current_ofs = scalar_ofs;
					scalar_ofs += sz.bytes;
				}
				std::map<block *, live_range> cr_copy;
				for (const auto &elem : current_range) {
					auto [b, lr] = elem.second;
					cr_copy.emplace(b, *lr);
				}
				slots.emplace_back(current_ofs, stack_slot{ sz, cr_copy });
			}
			for (instruction *i : p.second) {
				i->literals[0] = current_ofs;
			}
		}

		// stack must maintain 16-byte alignment
		if (scalar_ofs % 16 > 0) {
			scalar_ofs += 16 - (scalar_ofs % 16);
		}

		// make sure the total_ofs will fit in the immediate field of a add/sub
		// instruction (12-bits, optionally shifted left 12).
		unsigned total_ofs = scalar_ofs + child_call_arg_stack_size;
		if (total_ofs >= 0x1000u && (total_ofs & 0xfffu) != 0) {
			total_ofs = (total_ofs + 0x1000u) & ~0xfffu;
		}
		sloejit_assert((total_ofs & 0x000fffu) == total_ofs || (total_ofs & 0xfff000u) == total_ofs);

		// populate prologue with frame pointer maintenance
		aarch64::instr_builder ib{ prologue, prologue->instr_first };
		ib.make_x_sub_rri(aarch64::sp, aarch64::sp, total_ofs);
		ib.make_x_stp_rrri(aarch64::x29, aarch64::x30, aarch64::sp, child_call_arg_stack_size);
		ib.make_x_add_rri(aarch64::x29, aarch64::sp, child_call_arg_stack_size);
		for (int i = 0; i > vector_ofs; i -= 31) {
			int inc = std::min(31, i - vector_ofs);
			if (fn->opts.sme != function_options_t::sme_usage::none) {
				ib.make_addsvl_rri(aarch64::sp, aarch64::sp, -inc);
			} else {
				ib.make_addvl_rri(aarch64::sp, aarch64::sp, -inc);
			}
		}

		// setup block dependencies based on terminating instruction
		for (unsigned i = 0; i < fn->blocks.size(); ++i) {
			auto *b = &*fn->blocks[i];
			sloejit_assert(!b->instrs.empty());
			auto &instr = *b->instr_last;
			switch (instr.base->kind) {
			case sloejit::IK_NORMAL: sloejit_assert(false);
			case sloejit::IK_BRANCH_PCREL: break;
			case sloejit::IK_COND_BRANCH_PCREL: break;
			case sloejit::IK_RETURN: {
				// insert stack adjustment before ret instruction
				aarch64::instr_builder ib{ b, b->instr_last };
				for (int i = 0; i > vector_ofs; i -= 31) {
					int inc = std::min(31, i - vector_ofs);
					if (fn->opts.sme != function_options_t::sme_usage::none) {
						ib.make_addsvl_rri(aarch64::sp, aarch64::sp, inc);
					} else {
						ib.make_addvl_rri(aarch64::sp, aarch64::sp, inc);
					}
				}
				ib.make_x_ldp_rrri(aarch64::x29, aarch64::x30, aarch64::sp, child_call_arg_stack_size);
				ib.make_x_add_rri(aarch64::sp, aarch64::sp, total_ofs);
			} break;
			}
		}

		// search for non-spill stack accesses that need to be updated
		for (auto &b : fn->blocks) {
			for (auto &i : b->instrs) {
				if (!is_spill_fill_instr(&*i)) continue;
				if (all_spill_instrs.find(&*i) != all_spill_instrs.end()) continue;
				// some dubious logic here! assume that if the user is making a
				// load/store based on x29 then they want to reference the parent
				// args, and if they use sp then they want the child args!!!
				if (i->get_reg(1) == aarch64::x29) {
					sloejit_assert(i->literals.at(0) >= 0);
					sloejit_assert(i->literals.at(0) < parent_call_arg_stack_size);
					i->literals[0] += total_ofs - child_call_arg_stack_size;
				}
				else if (i->get_reg(1) == aarch64::sp) {
					sloejit_assert(i->literals.at(0) >= 0);
					sloejit_assert(i->literals.at(0) < child_call_arg_stack_size);
					// nothing to do here
				}
			}
		}
	}

	void post_regalloc_hook(function *fn) const override {
		for (auto &b : fn->blocks) {
			// iterate backwards through instructions in the block
			sloejit_assert(b->instr_first != nullptr);
			// keep track of what instruction last wrote to each register,
			// since if the instruction was a stack reload we may be able to avoid it.
			std::map<reg, instruction*> last_written;
			// keep track of which store instruction last touched a specific stack address,
			// since a store between two loads may mean we cannot reuse the loaded register.
			std::map<int, instruction*> spill_ofs_instrs;
			auto *instr = b->instr_first;
			auto *instr_next = instr->instr_next;
			for (; instr; instr = instr_next) {
				instr_next = instr->instr_next;
				if (aarch64_instr_is_redundant(instr)) {
					b->erase(instr);
					continue;
				}
				auto stack_reload_ofs = stack_reload_instr_get_ofs(instr);
				auto stack_spill_ofs = stack_spill_instr_get_ofs(instr);
				if (stack_reload_ofs) {
					sloejit_assert(instr->num_output_regs() == 1);
					sloejit_assert(instr->is_reg_output(0));
					reg r = instr->get_reg(0);
					auto lw_it = last_written.find(r);
					if (lw_it != last_written.end()) {
						auto spill_it = spill_ofs_instrs.find(*stack_reload_ofs);
						// there is an earlier reload and the address is not clobbered by a spill.
						bool can_erase = *lw_it->second == *instr &&
						                 (spill_it == spill_ofs_instrs.end() ||
						                  spill_it->second->pos < lw_it->second->pos);
						// the earlier spill to the same address has the same source register.
						can_erase = can_erase || (spill_it != spill_ofs_instrs.end() &&
						                          lw_it->second == spill_it->second &&
						                          spill_it->second->get_reg(0) == r);
						if (can_erase) {
							// instruction is redundant in context, delete it.
							b->erase(instr);
							continue;
						}
					}
				}
				for (unsigned i = 0; i < instr->nregs(); ++i) {
					if (instr->is_reg_output(i)) {
						last_written[instr->get_reg(i)] = instr;
					}
				}
				if (stack_spill_ofs) {
					last_written[instr->get_reg(0)] = instr;
					spill_ofs_instrs[*stack_spill_ofs] = instr;
				}
			}
		}
	}

	std::string opcode_to_string(int opcode) const override {
		return aarch64::opcode_to_string(opcode);
	}
};

const sloejit::arch_traits *aarch64::get_arch_traits() {
	static aarch64_traits t;
	return &t;
}
