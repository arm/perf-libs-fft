/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "sloejit/arch.hpp"
#include "sloejit/bytevector.hpp"
#include "sloejit/ir.hpp"

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace sloejit::aarch64 {

enum preg_spaces {
  x_space = 1,
  v_space,
  p_space,
  za_space,
};

enum preg_classes : uint8_t {
  x_regs = 0b1u,
  b_regs = 0b000001u,
  h_regs = 0b000011u,
  s_regs = 0b000111u,
  d_regs = 0b001111u,
  q_regs = 0b011111u,
  z_regs = 0b111111u,
  p_regs = 0b1u,
  za_regs = 0b1u,
};

enum q_type_variant {
  qv_8b = 1,
  qv_16b,
  qv_4h,
  qv_8h,
  qv_2s,
  qv_4s,
  qv_1d,
  qv_2d,
};

enum z_type_variant {
  zv_b = 1,
  zv_h,
  zv_s,
  zv_d,
  zv_q,
};

enum ptrue_pat {
  ptrue_pat_pow2 = 0b00000,
  ptrue_pat_vl1 = 0b00001,
  ptrue_pat_vl2 = 0b00010,
  ptrue_pat_vl3 = 0b00011,
  ptrue_pat_vl4 = 0b00100,
  ptrue_pat_vl5 = 0b00101,
  ptrue_pat_vl6 = 0b00110,
  ptrue_pat_vl7 = 0b00111,
  ptrue_pat_vl8 = 0b01000,
  ptrue_pat_vl16 = 0b01001,
  ptrue_pat_vl32 = 0b01010,
  ptrue_pat_vl64 = 0b01011,
  ptrue_pat_vl128 = 0b01100,
  ptrue_pat_vl256 = 0b01101,
  ptrue_pat_mul4 = 0b11101,
  ptrue_pat_mul3 = 0b11110,
  ptrue_pat_all = 0b11111,
};

enum shift_amount {
%for i in range(64):
  lsl_${i},
%endfor
};

enum smopt {
  smopt_sm = 1u,
  smopt_za,
  smopt_both,
};

enum hvopt {
  hvopt_h = 0,
  hvopt_v = 1,
};

inline constexpr reg xzr = { x_space, 1, x_regs };
inline constexpr reg sp = { x_space, 2, x_regs };
inline constexpr reg pc = { x_space, 3, x_regs };
%for i in range(31):
inline constexpr reg x${i} = { x_space, ${i+4}, x_regs };
%endfor
%for i in range(32):
inline constexpr reg b${i} = { v_space, ${i+1}, b_regs };
inline constexpr reg h${i} = { v_space, ${i+1}, h_regs };
inline constexpr reg s${i} = { v_space, ${i+1}, s_regs };
inline constexpr reg d${i} = { v_space, ${i+1}, d_regs };
inline constexpr reg q${i} = { v_space, ${i+1}, q_regs };
inline constexpr reg z${i} = { v_space, ${i+1}, z_regs };
inline constexpr reg z${i}_no_dreg = { v_space, ${i+1}, z_regs & ~d_regs };
%endfor
%for i in range(16):
inline constexpr reg p${i} = { p_space, ${i+1}, p_regs };
%endfor
inline constexpr reg za = { za_space, 1, za_regs };
/// where to split physical/virtual register spaces
inline constexpr uint64_t max_preg_num = 40;

uint64_t reg_get_space(reg reg);
reg reg_reinterpret_with_class(reg r, preg_classes active_mask);

struct instr_builder {
  block *b; ///< The block to insert into.
  instruction *instr_pos; ///< Instruction to insert before (or nullptr to append).
  std::optional<std::string> tag;

  constexpr instr_builder(block *b, instruction *instr_pos = nullptr)
    : b(b), instr_pos(instr_pos) {
  }

  instruction *get_last_inserted_instr() const {
    return instr_pos ? instr_pos->instr_prev : b->instr_last;
  }

  /// Arithmetic functions
  //@{
  [[nodiscard]]
  reg make_x_madd_rrr(reg rn, reg rm, reg ra);
  void make_x_madd_rrrr(reg rd, reg rn, reg rm, reg ra);
  [[nodiscard]]
  reg make_x_mul_rr(reg rn, reg rm);
  void make_x_mul_rrr(reg rd, reg rn, reg rm);
  // TODO: should probably support these, unused for now?
  // [[nodiscard]]
  // reg make_mul_zz(reg rn, reg rm, z_type_variant);
  // void make_mul_zzz(reg rd, reg rn, reg rm, z_type_variant);
  void make_mul_zpz(reg rd, reg pg, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_x_sub_rr(reg rn, reg rm, shift_amount);
  void make_x_sub_rrr(reg rd, reg rn, reg rm, shift_amount);
  [[nodiscard]]
  reg make_x_sub_ri(reg rn, uint32_t imm);
  void make_x_sub_rri(reg rd, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_sub_qq(reg rn, reg rm, q_type_variant);
  void make_sub_qqq(reg rd, reg rn, reg rm, q_type_variant);
  void make_sub_zi(reg rdn, uint32_t imm, z_type_variant);
  [[nodiscard]]
  reg make_sub_zz(reg rn, reg rm, z_type_variant);
  void make_sub_zzz(reg rd, reg rn, reg rm, z_type_variant);
  void make_sub_zpz(reg rd, reg pg, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_x_add_rr(reg rn, reg rm, shift_amount);
  void make_x_add_rrr(reg rd, reg rn, reg rm, shift_amount);
  [[nodiscard]]
  reg make_x_add_ri(reg rn, uint32_t imm);
  void make_x_add_rri(reg rd, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_x_add_rb(reg rn, branch_target *bt);
  void make_x_add_rrb(reg rd, reg rn, branch_target *bt);
  [[nodiscard]]
  reg make_add_qq(reg rn, reg rm, q_type_variant);
  void make_add_qqq(reg rd, reg rn, reg rm, q_type_variant);
  void make_add_zi(reg rdn, uint32_t imm, z_type_variant);
  [[nodiscard]]
  reg make_add_zz(reg rn, reg rm, z_type_variant);
  void make_add_zzz(reg rd, reg rn, reg rm, z_type_variant);
  void make_add_zpz(reg rd, reg pg, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_x_sdiv_rr(reg rn, reg rm);
  void make_x_sdiv_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_udiv_rr(reg rn, reg rm);
  void make_x_udiv_rrr(reg rd, reg rn, reg rm);

  // ASR is technically a special case of SBFM, but we don't need the full
  // behavior here.
  [[nodiscard]]
  reg make_asr_ri(reg rn, uint32_t imm);
  void make_asr_rri(reg rd, reg rn, uint32_t imm);

  // LSL/LSR are technically special cases of UBFM, but we don't need the full
  // behavior here.
  [[nodiscard]]
  reg make_lsl_ri(reg rn, uint32_t imm);
  void make_lsl_rri(reg rd, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_lsr_ri(reg rn, uint32_t imm);
  void make_lsr_rri(reg rd, reg rn, uint32_t imm);

  [[nodiscard]]
  reg make_fadd_hh(reg rn, reg rm);
  void make_fadd_hhh(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fadd_ss(reg rn, reg rm);
  void make_fadd_sss(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fadd_dd(reg rn, reg rm);
  void make_fadd_ddd(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fadd_qq(reg rn, reg rm, q_type_variant);
  void make_fadd_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_fadd_zz(reg rn, reg rm, z_type_variant);
  void make_fadd_zzz(reg rd, reg rn, reg rm, z_type_variant);
  void make_fadd_zpz(reg rdn, reg pg, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_fsub_hh(reg rn, reg rm);
  void make_fsub_hhh(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fsub_ss(reg rn, reg rm);
  void make_fsub_sss(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fsub_dd(reg rn, reg rm);
  void make_fsub_ddd(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fsub_qq(reg rn, reg rm, q_type_variant);
  void make_fsub_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_fsub_zz(reg rn, reg rm, z_type_variant);
  void make_fsub_zzz(reg rd, reg rn, reg rm, z_type_variant);
  void make_fsub_zpz(reg rdn, reg pg, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_fmul_hh(reg rn, reg rm);
  void make_fmul_hhh(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fmul_ss(reg rn, reg rm);
  void make_fmul_sss(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fmul_dd(reg rn, reg rm);
  void make_fmul_ddd(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_fmul_qq(reg rn, reg rm, q_type_variant);
  void make_fmul_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_fmul_qql(reg rn, reg rm, int lane, q_type_variant);
  void make_fmul_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant);
  [[nodiscard]]
  reg make_fmul_zz(reg rn, reg rm, z_type_variant);
  void make_fmul_zzz(reg rd, reg rn, reg rm, z_type_variant);
  void make_fmul_zpz(reg rdn, reg pg, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_fneg_h(reg rn);
  void make_fneg_hh(reg rd, reg rn);
  [[nodiscard]]
  reg make_fneg_s(reg rn);
  void make_fneg_ss(reg rd, reg rn);
  [[nodiscard]]
  reg make_fneg_d(reg rn);
  void make_fneg_dd(reg rd, reg rn);
  [[nodiscard]]
  reg make_fneg_q(reg rn, q_type_variant);
  void make_fneg_qq(reg rd, reg rn, q_type_variant);
  void make_fneg_zpz(reg rd, reg pg, reg rn, z_type_variant);

  void make_fmla_qqq(reg rd, reg rn, reg rm, q_type_variant);
  void make_fmla_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant);
  void make_fmla_zpzz(reg rda, reg pg, reg rn, reg rm, z_type_variant);
  void make_fmla_zzzl(reg rda, reg rn, reg rm, int lane, z_type_variant);
  void make_fmls_qqq(reg rd, reg rn, reg rm, q_type_variant);
  void make_fmls_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant);
  void make_fmls_zpzz(reg rda, reg pg, reg rn, reg rm, z_type_variant);
  void make_fmls_zzzl(reg rda, reg rn, reg rm, int lane, z_type_variant);

  void make_fcmla_qqqi(reg rd, reg rn, reg rm, int rot, q_type_variant);
  void make_fcmla_zpzzi(reg rda, reg pg, reg rn, reg rm, int rot, z_type_variant);
  //@}

  /// Arithmetic functions - Flag setting
  //@{
  [[nodiscard]]
  reg make_x_subs_rr(reg rn, reg rm);
  void make_x_subs_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_subs_ri(reg rn, uint32_t imm);
  void make_x_subs_rri(reg rd, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_x_adds_rr(reg rn, reg rm);
  void make_x_adds_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_adds_ri(reg rn, uint32_t imm);
  void make_x_adds_rri(reg rd, reg rn, uint32_t imm);
  //@}

  /// Arithmetic functions - SVE vector-length aware
  //@{
  [[nodiscard]]
  reg make_cntb();
  void make_cntb_r(reg rd);
  [[nodiscard]]
  reg make_cnth();
  void make_cnth_r(reg rd);
  [[nodiscard]]
  reg make_cntw();
  void make_cntw_r(reg rd);
  [[nodiscard]]
  reg make_cntd();
  void make_cntd_r(reg rd);

  void make_cntp_rpp(reg rd, reg pg, reg pn, z_type_variant);

  [[nodiscard]]
  reg make_incb();
  void make_incb_r(reg rd);
  [[nodiscard]]
  reg make_inch();
  void make_inch_r(reg rd);
  [[nodiscard]]
  reg make_incw();
  void make_incw_r(reg rd);
  [[nodiscard]]
  reg make_incd();
  void make_incd_r(reg rd);

  [[nodiscard]]
  reg make_addvl_ri(reg rn, int imm);
  void make_addvl_rri(reg rd, reg rn, int imm);
  [[nodiscard]]
  reg make_addsvl_ri(reg rn, int imm);
  void make_addsvl_rri(reg rd, reg rn, int imm);

  [[nodiscard]]
  reg make_index_ii(int imm, int immb, z_type_variant);
  void make_index_zii(reg rd, int imm, int immb, z_type_variant);
  [[nodiscard]]
  reg make_index_ir(int imm, reg rm, z_type_variant);
  void make_index_zir(reg rd, int imm, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_index_ri(reg rn, int imm, z_type_variant);
  void make_index_zri(reg rd, reg rn, int imm, z_type_variant);
  [[nodiscard]]
  reg make_index_rr(reg rn, reg rm, z_type_variant);
  void make_index_zrr(reg rd, reg rn, reg rm, z_type_variant);
  //@}

  /// Arithmetic functions - widening multiply instructions
  //@{
  void make_smlalb_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from);
  void make_smlalb_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  void make_smlal_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from);
  void make_smlal_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  void make_smlal2_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from);
  void make_smlal2_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  void make_smlalt_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from);
  void make_smlalt_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  void make_smlslb_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from);
  void make_smlslb_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  void make_smlsl_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from);
  void make_smlsl_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  void make_smlsl2_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from);
  void make_smlsl2_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  void make_smlslt_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from);
  void make_smlslt_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  [[nodiscard]]
  reg make_smullb_zz(reg rn, reg rm, z_type_variant to, z_type_variant from);
  void make_smullb_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from);
  [[nodiscard]]
  reg make_smullb_zzl(reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  void make_smullb_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  [[nodiscard]]
  reg make_smull_qq(reg rn, reg rm, q_type_variant to, q_type_variant from);
  void make_smull_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from);
  [[nodiscard]]
  reg make_smull_qql(reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  void make_smull_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  [[nodiscard]]
  reg make_smull2_qq(reg rn, reg rm, q_type_variant to, q_type_variant from);
  void make_smull2_qqq(reg rd, reg rn, reg rm, q_type_variant to, q_type_variant from);
  [[nodiscard]]
  reg make_smull2_qql(reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  void make_smull2_qqql(reg rd, reg rn, reg rm, int lane, q_type_variant to, q_type_variant from);
  [[nodiscard]]
  reg make_smullt_zz(reg rn, reg rm, z_type_variant to, z_type_variant from);
  void make_smullt_zzz(reg rd, reg rn, reg rm, z_type_variant to, z_type_variant from);
  [[nodiscard]]
  reg make_smullt_zzl(reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  void make_smullt_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant to, z_type_variant from);
  //@}

  /// Arithmetic functions - saturating arithmetic
  //@{
  [[nodiscard]]
  reg make_sqadd_qq(reg rn, reg rm, q_type_variant);
  void make_sqadd_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_sqadd_zz(reg rn, reg rm, z_type_variant);
  void make_sqadd_zzz(reg rd, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_sqneg_pz(reg pg, reg rn, z_type_variant);
  void make_sqneg_zpz(reg rd, reg pg, reg rn, z_type_variant);
  [[nodiscard]]
  reg make_sqneg_q(reg rn, q_type_variant);
  void make_sqneg_qq(reg rd, reg rn, q_type_variant);
  void make_sqrdcmlah_zzzi(reg rda, reg rn, reg rm, int rot, z_type_variant zv);
  void make_sqrdmlah_qqq(reg rda, reg rn, reg rm, q_type_variant);
  void make_sqrdmlah_qqql(reg rda, reg rn, reg rm, unsigned lane, q_type_variant);
  void make_sqrdmlah_zzz(reg rda, reg rn, reg rm, z_type_variant);
  void make_sqrdmlah_zzzl(reg rda, reg rn, reg rm, unsigned lane, z_type_variant);
  void make_sqrdmlsh_qqq(reg rda, reg rn, reg rm, q_type_variant);
  void make_sqrdmlsh_qqql(reg rda, reg rn, reg rm, unsigned lane, q_type_variant);
  void make_sqrdmlsh_zzz(reg rda, reg rn, reg rm, z_type_variant);
  void make_sqrdmlsh_zzzl(reg rda, reg rn, reg rm, unsigned lane, z_type_variant);
  void make_sqrdmulh_qqq(reg rda, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_sqrdmulh_qql(reg rn, reg rm, unsigned lane, q_type_variant);
  void make_sqrdmulh_qqql(reg rd, reg rn, reg rm, unsigned lane, q_type_variant);
  void make_sqrdmulh_zzz(reg rda, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_sqrdmulh_zzl(reg rn, reg rm, unsigned lane, z_type_variant);
  void make_sqrdmulh_zzzl(reg rd, reg rn, reg rm, unsigned lane, z_type_variant);
  void make_sqrshrnb_zzi(reg rdn, reg rn, unsigned imm, z_type_variant to, z_type_variant from);
  void make_sqrshrnt_zzi(reg rdn, reg rn, unsigned imm, z_type_variant to, z_type_variant from);
  [[nodiscard]]
  reg make_sqsub_qq(reg rn, reg rm, q_type_variant);
  void make_sqsub_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_sqsub_zz(reg rn, reg rm, z_type_variant);
  void make_sqsub_zzz(reg rd, reg rn, reg rm, z_type_variant);
  //@}

  /// Comparisons
  //@{
  void make_x_cmp_rr(reg rn, reg rm);
  void make_x_cmp_ri(reg rn, int imm);
  //@}

  /// Logical functions
  //@{
  [[nodiscard]]
  reg make_x_and_rr(reg rn, reg rm);
  void make_x_and_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_and_qq(reg rn, reg rm, q_type_variant qv);
  void make_and_qqq(reg rd, reg rn, reg rm, q_type_variant qv);
  [[nodiscard]]
  reg make_and_zz(reg rn, reg rm);
  void make_and_zzz(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_orr_rr(reg rn, reg rm);
  void make_x_orr_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_orr_qq(reg rn, reg rm, q_type_variant qv);
  void make_orr_qqq(reg rd, reg rn, reg rm, q_type_variant qv);
  [[nodiscard]]
  reg make_orr_zz(reg rn, reg rm);
  void make_orr_zzz(reg rd, reg rn, reg rm);

  [[nodiscard]]
  reg make_x_csel_eq_rr(reg rn, reg rm);
  void make_x_csel_eq_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_csel_ne_rr(reg rn, reg rm);
  void make_x_csel_ne_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_csel_le_rr(reg rn, reg rm);
  void make_x_csel_le_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_csel_lt_rr(reg rn, reg rm);
  void make_x_csel_lt_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_csel_ge_rr(reg rn, reg rm);
  void make_x_csel_ge_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_csel_gt_rr(reg rn, reg rm);
  void make_x_csel_gt_rrr(reg rd, reg rn, reg rm);
  //@}

  /// Vector shuffles
  //@{
  [[nodiscard]]
  reg make_rev16_q(reg rn, q_type_variant);
  void make_rev16_qq(reg rd, reg rn, q_type_variant);
  [[nodiscard]]
  reg make_rev32_q(reg rn, q_type_variant);
  void make_rev32_qq(reg rd, reg rn, q_type_variant);
  [[nodiscard]]
  reg make_rev64_q(reg rn, q_type_variant);
  void make_rev64_qq(reg rd, reg rn, q_type_variant);
  void make_revb_zpz(reg rd, reg pg, reg rn, z_type_variant);
  void make_revh_zpz(reg rd, reg pg, reg rn, z_type_variant);
  void make_revw_zpz(reg rd, reg pg, reg rn, z_type_variant);
  [[nodiscard]] reg make_rev_z(reg rn, z_type_variant);
  void make_rev_zz(reg rd, reg rn, z_type_variant);
  [[nodiscard]] reg make_rev_p(reg pn, z_type_variant);
  void make_rev_pp(reg pd, reg pn, z_type_variant);
  void make_ext_zzi(reg rdn, reg rm, unsigned idx);
  [[nodiscard]]
  reg make_ext_qq(reg rn, reg rm, int idx, q_type_variant);
  void make_ext_qqq(reg rd, reg rn, reg rm, int idx, q_type_variant);

  [[nodiscard]]
  reg make_trn1_qq(reg rn, reg rm, q_type_variant);
  void make_trn1_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_trn1_zz(reg rn, reg rm, z_type_variant);
  void make_trn1_zzz(reg rd, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_trn2_qq(reg rn, reg rm, q_type_variant);
  void make_trn2_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_trn2_zz(reg rn, reg rm, z_type_variant);
  void make_trn2_zzz(reg rd, reg rn, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_uzp1_qq(reg rn, reg rm, q_type_variant);
  void make_uzp1_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_uzp1_zz(reg rn, reg rm, z_type_variant);
  void make_uzp1_zzz(reg rd, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_uzp2_qq(reg rn, reg rm, q_type_variant);
  void make_uzp2_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_uzp2_zz(reg rn, reg rm, z_type_variant);
  void make_uzp2_zzz(reg rd, reg rn, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_zip1_qq(reg rn, reg rm, q_type_variant);
  void make_zip1_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_zip1_zz(reg rn, reg rm, z_type_variant);
  void make_zip1_zzz(reg rd, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_zip2_qq(reg rn, reg rm, q_type_variant);
  void make_zip2_qqq(reg rd, reg rn, reg rm, q_type_variant);
  [[nodiscard]]
  reg make_zip2_zz(reg rn, reg rm, z_type_variant);
  void make_zip2_zzz(reg rd, reg rn, reg rm, z_type_variant);

  void make_splice_zpz(reg rdn, reg pv, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_dup_zi(int imm, z_type_variant);
  void make_dup_zi(reg rd, int imm, z_type_variant);
  [[nodiscard]]
  reg make_dup_zl(reg rn, int lane, z_type_variant);
  void make_dup_zzl(reg rd, reg rn, int lane, z_type_variant);

  void make_dup_qr(reg rd, reg rn, q_type_variant);
  [[nodiscard]]
  reg make_dup_ql(reg rn, int lane, q_type_variant);
  void make_dup_qql(reg rd, reg rn, int lane, q_type_variant);
  //@}

  /// Floating-point conversions
  //@{
  [[nodiscard]]
  reg make_fcvt_sh(reg rn);
  void make_fcvt_sh(reg rd, reg rn);
  [[nodiscard]]
  reg make_fcvt_dh(reg rn);
  void make_fcvt_dh(reg rd, reg rn);
  [[nodiscard]]
  reg make_fcvt_hs(reg rn);
  void make_fcvt_hs(reg rd, reg rn);
  [[nodiscard]]
  reg make_fcvt_ds(reg rn);
  void make_fcvt_ds(reg rd, reg rn);
  [[nodiscard]]
  reg make_fcvt_hd(reg rn);
  void make_fcvt_hd(reg rd, reg rn);
  [[nodiscard]]
  reg make_fcvt_sd(reg rn);
  void make_fcvt_sd(reg rd, reg rn);
  [[nodiscard]]
  reg make_fcvtn_q(reg rn, q_type_variant to, q_type_variant from);
  void make_fcvtn_qq(reg rd, reg rn, q_type_variant to, q_type_variant from);
  [[nodiscard]]
  reg make_fcvtl_q(reg rn, q_type_variant to, q_type_variant from);
  void make_fcvtl_qq(reg rd, reg rn, q_type_variant to, q_type_variant from);
  //@}

  /// Predicate manipulation
  //@{
  [[nodiscard]]
  reg make_pfalse();
  void make_pfalse(reg rd);

  [[nodiscard]]
  reg make_ptrue(z_type_variant);
  void make_ptrue(reg rd, z_type_variant);
  [[nodiscard]]
  reg make_ptrue(ptrue_pat, z_type_variant);
  void make_ptrue(reg rd, ptrue_pat, z_type_variant);

  [[nodiscard]]
  reg make_whilelt_rr(reg rn, reg rm, z_type_variant);
  void make_whilelt_prr(reg pd, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_whilele_rr(reg rn, reg rm, z_type_variant);
  void make_whilele_prr(reg pd, reg rn, reg rm, z_type_variant);

  [[nodiscard]]
  reg make_trn1_pp(reg pn, reg pm, z_type_variant);
  void make_trn1_ppp(reg pd, reg pn, reg pm, z_type_variant);
  [[nodiscard]]
  reg make_trn2_pp(reg pn, reg pm, z_type_variant);
  void make_trn2_ppp(reg pd, reg pn, reg pm, z_type_variant);

  [[nodiscard]]
  reg make_zip1_pp(reg pn, reg pm, z_type_variant);
  void make_zip1_ppp(reg pd, reg pn, reg pm, z_type_variant);
  [[nodiscard]]
  reg make_zip2_pp(reg pn, reg pm, z_type_variant);
  void make_zip2_ppp(reg pd, reg pn, reg pm, z_type_variant);

  [[nodiscard]]
  reg make_uzp1_pp(reg pn, reg pm, z_type_variant);
  void make_uzp1_ppp(reg pd, reg pn, reg pm, z_type_variant);
  [[nodiscard]]
  reg make_uzp2_pp(reg pn, reg pm, z_type_variant);
  void make_uzp2_ppp(reg pd, reg pn, reg pm, z_type_variant);
  //@}

  /// Loads
  //@{
  [[nodiscard]]
  reg make_adr_b(branch_target *bt);
  void make_adr_rb(reg rd, branch_target *bt);
  [[nodiscard]]
  reg make_adr_i(int64_t ofs);
  void make_adr_ri(reg rd, int64_t ofs);
  [[nodiscard]]
  reg make_adrp_b(branch_target *bt);
  void make_adrp_rb(reg rd, branch_target *bt);
  [[nodiscard]]
  reg make_adrp_i(int64_t ofs);
  void make_adrp_ri(reg rd, int64_t ofs);

  [[nodiscard]]
  reg make_x_ldrsb_rr(reg rn, reg rm);
  void make_x_ldrsb_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_ldrsb_ri(reg rn, uint32_t imm);
  void make_x_ldrsb_rri(reg rd, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_x_ldr_rr(reg rn, reg rm);
  void make_x_ldr_rrr(reg rd, reg rn, reg rm);
  [[nodiscard]]
  reg make_x_ldr_ri(reg rn, uint32_t imm);
  void make_x_ldr_rri(reg rd, reg rn, uint32_t imm);

  void make_x_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm);
  void make_x_ldp_preindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm);
  void make_x_ldp_postindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm);

  [[nodiscard]]
  reg make_b_ldr_rr(reg rn, reg rm);
  void make_b_ldr_rrr(reg rt, reg rn, reg rm);
  [[nodiscard]]
  reg make_b_ldr_ri(reg rn, uint32_t imm);
  void make_b_ldr_rri(reg rt, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_h_ldr_rr(reg rn, reg rm);
  void make_h_ldr_rrr(reg rt, reg rn, reg rm);
  [[nodiscard]]
  reg make_h_ldr_ri(reg rn, uint32_t imm);
  void make_h_ldr_rri(reg rt, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_s_ldr_rr(reg rn, reg rm);
  void make_s_ldr_rrr(reg rt, reg rn, reg rm);
  [[nodiscard]]
  reg make_s_ldr_ri(reg rn, uint32_t imm);
  void make_s_ldr_rri(reg rt, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_d_ldr_rr(reg rn, reg rm);
  void make_d_ldr_rrr(reg rt, reg rn, reg rm);
  [[nodiscard]]
  reg make_d_ldr_ri(reg rn, uint32_t imm);
  void make_d_ldr_rri(reg rt, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_q_ldr_rr(reg rn, reg rm);
  void make_q_ldr_rrr(reg rt, reg rn, reg rm);
  [[nodiscard]]
  reg make_q_ldr_ri(reg rn, uint32_t imm);
  void make_q_ldr_rri(reg rt, reg rn, uint32_t imm);
  [[nodiscard]]
  reg make_ldr_zri(reg rn, int32_t imm);
  void make_ldr_zri(reg rt, reg rn, int32_t imm);

  [[nodiscard]]
  std::pair<reg, reg> make_s_ldp_ri(reg rn, int32_t imm);
  void make_s_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm);
  [[nodiscard]]
  std::pair<reg, reg> make_d_ldp_ri(reg rn, int32_t imm);
  void make_d_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm);
  [[nodiscard]]
  std::pair<reg, reg> make_q_ldp_ri(reg rn, int32_t imm);
  void make_q_ldp_rrri(reg rt1, reg rt2, reg rn, int32_t imm);

  [[nodiscard]]
  reg make_ld1b_pri(reg pg, reg rn, int32_t imm, z_type_variant);
  void make_ld1b_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  [[nodiscard]]
  reg make_ld1b_prr(reg pg, reg rn, reg rm, z_type_variant);
  void make_ld1b_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_ld1b_prz(reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_ld1b_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  [[nodiscard]]
  reg make_ld1h_pri(reg pg, reg rn, int32_t imm, z_type_variant);
  void make_ld1h_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  [[nodiscard]]
  reg make_ld1h_prr(reg pg, reg rn, reg rm, z_type_variant);
  void make_ld1h_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_ld1h_prz(reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_ld1h_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  [[nodiscard]]
  reg make_ld1w_pri(reg pg, reg rn, int32_t imm, z_type_variant);
  void make_ld1w_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  [[nodiscard]]
  reg make_ld1w_prr(reg pg, reg rn, reg rm, z_type_variant);
  void make_ld1w_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_ld1w_prz(reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_ld1w_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  [[nodiscard]]
  reg make_ld1d_pri(reg pg, reg rn, int32_t imm, z_type_variant);
  void make_ld1d_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  [[nodiscard]]
  reg make_ld1d_prr(reg pg, reg rn, reg rm, z_type_variant);
  void make_ld1d_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  [[nodiscard]]
  reg make_ld1d_prz(reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_ld1d_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);

  void make_ld1b_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t off4, reg pg, reg rn);
  void make_ld1b_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t off4, reg pg, reg rn, reg rm);
  void make_ld1h_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t off3, reg pg, reg rn);
  void make_ld1h_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t off3, reg pg, reg rn, reg rm);
  void make_ld1w_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_ld1w_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  void make_ld1d_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_ld1d_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  void make_ld1q_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_ld1q_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);

  [[nodiscard]]
  reg make_ld1r_r(reg rn, q_type_variant);
  void make_ld1r_rr(reg rt, reg rn, q_type_variant);

  void make_ld2_rrr(reg rt1, reg rt2, reg rn, q_type_variant);
  void make_ld3_rrrr(reg rt1, reg rt2, reg rt3, reg rn, q_type_variant);
  void make_ld4_rrrrr(reg rt1, reg rt2, reg rt3, reg rt4, reg rn, q_type_variant);
  void make_st2_rrr(reg rt1, reg rt2, reg rn, q_type_variant);
  void make_st3_rrrr(reg rt1, reg rt2, reg rt3, reg rn, q_type_variant);
  void make_st4_rrrrr(reg rt1, reg rt2, reg rt3, reg rt4, reg rn, q_type_variant);

  [[nodiscard]]
  reg make_ld1rb_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rb_zpri(reg rt, reg pg, reg rn, int32_t imm);
  [[nodiscard]]
  reg make_ld1rh_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rh_zpri(reg rt, reg pg, reg rn, int32_t imm);
  [[nodiscard]]
  reg make_ld1rw_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rw_zpri(reg rt, reg pg, reg rn, int32_t imm);
  [[nodiscard]]
  reg make_ld1rd_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rd_zpri(reg rt, reg pg, reg rn, int32_t imm);

  [[nodiscard]]
  reg make_ld1rqb_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rqb_zpri(reg rt, reg pg, reg rn, int32_t imm);
  [[nodiscard]]
  reg make_ld1rqh_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rqh_zpri(reg rt, reg pg, reg rn, int32_t imm);
  [[nodiscard]]
  reg make_ld1rqw_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rqw_zpri(reg rt, reg pg, reg rn, int32_t imm);
  [[nodiscard]]
  reg make_ld1rqd_pri(reg pg, reg rn, int32_t imm);
  void make_ld1rqd_zpri(reg rt, reg pg, reg rn, int32_t imm);

  void make_ld2b_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_ld2b_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_ld2h_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_ld2h_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_ld2w_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_ld2w_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_ld2d_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_ld2d_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_ld2q_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_ld2q_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_ld3b_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_ld3b_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_ld3h_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_ld3h_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_ld3w_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_ld3w_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_ld3d_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_ld3d_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_ld3q_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_ld3q_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_ld4b_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_ld4b_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_ld4h_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_ld4h_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_ld4w_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_ld4w_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_ld4d_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_ld4d_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_ld4q_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_ld4q_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);

  //@}

  /// Stores
  //@{
  void make_x_strb_rrr(reg rt, reg rn, reg rm);
  void make_x_strb_rri(reg rt, reg rn, uint32_t imm);
  void make_x_str_rrr(reg rt, reg rn, reg rm);
  void make_x_str_rri(reg rt, reg rn, uint32_t imm);

  void make_x_stp_rrri(reg rt1, reg rt2, reg rn, int32_t imm);
  void make_x_stp_preindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm);
  void make_x_stp_postindex_rrri(reg rt1, reg rt2, reg rn, int32_t imm);

  void make_b_str_rri(reg rt, reg rn, uint32_t imm);
  void make_b_str_rrr(reg rt, reg rn, reg rm);
  void make_h_str_rri(reg rt, reg rn, uint32_t imm);
  void make_h_str_rrr(reg rt, reg rn, reg rm);
  void make_s_str_rri(reg rt, reg rn, uint32_t imm);
  void make_s_str_rrr(reg rt, reg rn, reg rm);
  void make_d_str_rri(reg rt, reg rn, uint32_t imm);
  void make_d_str_rrr(reg rt, reg rn, reg rm);
  void make_q_str_rri(reg rt, reg rn, uint32_t imm);
  void make_q_str_rrr(reg rt, reg rn, reg rm);
  void make_str_zri(reg rt, reg rn, int32_t imm);

  void make_q_st1_rir(reg rt, int idx, reg rn, q_type_variant qv);

  void make_st1b_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  void make_st1b_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  void make_st1b_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_st1h_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  void make_st1h_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  void make_st1h_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_st1w_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  void make_st1w_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  void make_st1w_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);
  void make_st1d_zpri(reg rt, reg pg, reg rn, int32_t imm, z_type_variant);
  void make_st1d_zprr(reg rt, reg pg, reg rn, reg rm, z_type_variant);
  void make_st1d_zprz(reg rt, reg pg, reg rn, reg rm, shift_amount, z_type_variant);

  void make_st2b_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_st2b_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_st2h_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_st2h_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_st2w_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_st2w_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_st2d_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_st2d_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_st2q_zzpri(reg zt1, reg zt2, reg pg, reg rn, int32_t imm);
  void make_st2q_zzprr(reg zt1, reg zt2, reg pg, reg rn, reg rm);
  void make_st3b_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_st3b_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_st3h_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_st3h_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_st3w_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_st3w_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_st3d_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_st3d_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_st3q_zzzpri(reg zt1, reg zt2, reg zt3, reg pg, reg rn, int32_t imm);
  void make_st3q_zzzprr(reg zt1, reg zt2, reg zt3, reg pg, reg rn, reg rm);
  void make_st4b_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_st4b_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_st4h_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_st4h_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_st4w_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_st4w_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_st4d_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_st4d_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);
  void make_st4q_zzzzpri(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, int32_t imm);
  void make_st4q_zzzzprr(reg zt1, reg zt2, reg zt3, reg zt4, reg pg, reg rn, reg rm);

  void make_st1b_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_st1b_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  void make_st1h_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_st1h_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  void make_st1w_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  void make_st1w_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_st1d_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  void make_st1d_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_st1q_alorlpr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn);
  void make_st1q_alorlprr(reg za, uint32_t t, uint32_t v, reg rs, uint32_t o1, reg pg, reg rn, reg rm);
  //@}

  /// Literal Movs
  //@{
  [[nodiscard]]
  reg make_x_movz_i(uint64_t imm);
  void make_x_movz_ri(reg rd, uint64_t imm);
  [[nodiscard]]
  reg make_x_movn_i(uint64_t imm);
  void make_x_movn_ri(reg rd, uint64_t imm);
  void make_x_movk_ri(reg rd, uint64_t imm);
  //@}

  // Tile-slice <-> vector MOVs
  void make_mova_zpalorl(reg zd, reg pg, reg za, uint32_t tile, uint32_t hv, reg rs, uint32_t off, z_type_variant);
  void make_mova_alorlpz(reg za, uint32_t tile, uint32_t hv, reg rs, uint32_t off, reg pg, reg zn, z_type_variant);

  /// Local Branches
  //@{
  // TODO: these could technically take functions as targets. this is
  //       intentional to permit tail calls, but they don't currently
  //       take argument lists which means it won't work just yet.
  void make_cbnz_ri(reg rt, branch_target *bt);
  void make_cbz_ri(reg rt, branch_target *bt);
  void make_b_i(branch_target *bt);

  void make_b_eq_i(int64_t imm);
  void make_b_eq_b(branch_target *bt);
  void make_b_ne_i(int64_t imm);
  void make_b_ne_b(branch_target *bt);
  void make_b_le_i(int64_t imm);
  void make_b_le_b(branch_target *bt);
  void make_b_lt_i(int64_t imm);
  void make_b_lt_b(branch_target *bt);
  void make_b_ge_i(int64_t imm);
  void make_b_ge_b(branch_target *bt);
  void make_b_gt_i(int64_t imm);
  void make_b_gt_b(branch_target *bt);
  //@}

  /// Make a remote call, with the specified arguments as inputs/outputs.
  void make_bl_i(function *fn, std::vector<reg> regs, std::vector<bool> input_mask, std::vector<bool> output_mask);

  /// Make a remote indirect call, with the specified arguments as inputs/outputs.
  void make_blr_r(reg rn, std::vector<reg> regs, std::vector<bool> input_mask, std::vector<bool> output_mask);

  /// Register to register moves
  //@{
  [[nodiscard]]
  reg make_fmov_rh(reg rn);
  void make_fmov_rh(reg rd, reg rn);
  [[nodiscard]]
  reg make_fmov_rs(reg rn);
  void make_fmov_rs(reg rd, reg rn);
  [[nodiscard]]
  reg make_fmov_rd(reg rn);
  void make_fmov_rd(reg rd, reg rn);
  [[nodiscard]]
  reg make_fmov_hr(reg rn);
  void make_fmov_hr(reg rd, reg rn);
  [[nodiscard]]
  reg make_fmov_sr(reg rn);
  void make_fmov_sr(reg rd, reg rn);
  [[nodiscard]]
  reg make_fmov_dr(reg rn);
  void make_fmov_dr(reg rd, reg rn);
  [[nodiscard]]
  reg make_smov_ql(reg rn, int lane, q_type_variant qv);
  void make_smov_rql(reg rd, reg rn, int lane, q_type_variant qv);

  void make_x_mov_rr(reg rd, reg rm);
  //@}

  void make_ret();

  void make_smstart();
  void make_smstart_i(smopt opt);
  void make_smstop();
  void make_smstop_i(smopt opt);

  /// Saturating rounded right shift instructions
  //@{
  [[nodiscard]]
  reg make_sqrshrn_qi(reg rn, unsigned imm, q_type_variant to, q_type_variant from);
  void make_sqrshrn_qqi(reg rd, reg rn, unsigned imm, q_type_variant to, q_type_variant from);
  [[nodiscard]]
  reg make_sqrshrn2_qi(reg rn, unsigned imm, q_type_variant to, q_type_variant from);
  void make_sqrshrn2_qqi(reg rd, reg rn, unsigned imm, q_type_variant dest, q_type_variant from);
  [[nodiscard]]
  reg make_srshr_qi(reg rn, unsigned imm, q_type_variant);
  void make_srshr_qqi(reg rd, reg rn, unsigned imm, q_type_variant);
  void make_srshr_zpi(reg rdn, reg pg, unsigned imm, z_type_variant);
  //@}
};

const arch_traits* get_arch_traits();

}
