/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irprinter_utils.hpp"
#include "irvalue_scope.hpp"
#include "plfft_assert.hpp"
#include "registers.hpp"

#include <optional>

namespace plfft::wfta {

static inline sloejit::reg z_zero(sloejit::aarch64::instr_builder &ib) {
  return ib.make_dup_zi(0, sloejit::aarch64::zv_d);
}

static inline void z_zero(sloejit::reg r, sloejit::aarch64::instr_builder &ib) {
  ib.make_dup_zi(r, 0, sloejit::aarch64::zv_d);
}

static inline void emit_imul(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->nelems.is_one());
  auto dst_r = x_(ib, regmap, v);
  // try to replace a sequence such as:
  // movz x0, #(1 << n); mul x2, x1, x0
  // with
  // lsl x2, x1, #n
  if (get_pow2_lsl_int(a)) {
    std::swap(a, b);
  }
  auto shift_val = get_pow2_lsl_int(b);
  if (shift_val) {
    // We've got the shift, and now we want to emit an lsl instruction
    auto ra = x_(ib, regmap, a);
    ib.make_lsl_rri(dst_r, ra, *shift_val);
    return;
  }
  // try to replace a sequence such as:
  // movz x0, #((1 << n) + 1); mul x2, x1, x0
  // with
  // add x2, x1, x1, lsl #n
  if (get_pow2_plus1_lsl(a)) {
    std::swap(a, b);
  }
  auto shift_val2 = get_pow2_plus1_lsl(b);
  if (shift_val2) {
    // We've got the shift, and now we want to emit an lsl instruction
    auto ra = x_(ib, regmap, a);
    ib.make_x_add_rrr(dst_r, ra, ra, *shift_val2);
    return;
  }
  auto ra = x_(ib, regmap, a);
  auto rb = x_(ib, regmap, b);
  ib.make_x_mul_rrr(dst_r, ra, rb);
}

template<bool IsSVE>
static inline void emit_fadd_fsub(sloejit::aarch64::instr_builder &ib,
                                  std::vector<rodata_info> &data_ofs,
                                  std::vector<uint8_t> &data_bytes,
                                  regmap_t &regmap, ir_value v) {
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(v->deps.size() == 2);
  ASSERT(a->type->nelems == b->type->nelems);

  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto vec_r = get_qzv<IsSVE>(v->type.get());

  if ((v->op == IVO_FADD && a->op == IVO_FMUL) || b->op == IVO_FMUL) {
    auto fmul_op = b->op == IVO_FMUL ? b : a;
    auto acc_op = b->op == IVO_FMUL ? a : b;
    auto acc_reg_it = regmap.find(acc_op->id);
    // if acc is not used later in the program, see if we can fuse
    // a multiply and add
    if (acc_reg_it == regmap.end()) {
      regmap[acc_op->id].reg = dst_r;
      auto fmul_a = fmul_op->deps[0];
      auto fmul_b = fmul_op->deps[1];
      if (is_dup(fmul_a)) {
        std::swap(fmul_a, fmul_b);
      }
      auto rfmul_a =
          get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, fmul_a);
      if (is_dup(fmul_b)) {
        // if one of the arguments to the fmul is a broadcast, then we can
        // use indexed fmla.
        ASSERT(fmul_b->deps.size() == 1);
        auto dup_arg = fmul_b->deps[0];
        auto dup_idx = (int)fmul_b->literals[0];
        auto dup_arg_r =
            get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, dup_arg);
        if constexpr (IsSVE) {
          v->op == IVO_FADD
              ? ib.make_fmla_zzzl(dst_r, rfmul_a, dup_arg_r, dup_idx, vec_r)
              : ib.make_fmls_zzzl(dst_r, rfmul_a, dup_arg_r, dup_idx, vec_r);
        } else {
          v->op == IVO_FADD
              ? ib.make_fmla_qqql(dst_r, rfmul_a, dup_arg_r, dup_idx, vec_r)
              : ib.make_fmls_qqql(dst_r, rfmul_a, dup_arg_r, dup_idx, vec_r);
        }
        // If the fadd/sub is the only use of the fmul, remove the fmul as a
        // use of the broadcast. We need the check here since otherwise the
        // fmul may still be emitted due to any other uses.
        if (fmul_op->uses.size() == 1) {
          fmul_b->erase_use(fmul_op);
        }
      } else {
        auto rfmul_b =
            get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, fmul_b);
        if constexpr (IsSVE) {
          v->op == IVO_FADD
              ? ib.make_fmla_zpzz(dst_r, p_true, rfmul_a, rfmul_b, vec_r)
              : ib.make_fmls_zpzz(dst_r, p_true, rfmul_a, rfmul_b, vec_r);
        } else {
          v->op == IVO_FADD ? ib.make_fmla_qqq(dst_r, rfmul_a, rfmul_b, vec_r)
                            : ib.make_fmls_qqq(dst_r, rfmul_a, rfmul_b, vec_r);
        }
      }
      // Remove the add as a use of the fmul
      fmul_op->erase_use(v);
      return;
    }
  }

  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  if constexpr (IsSVE) {
    v->op == IVO_FADD ? ib.make_fadd_zzz(dst_r, ra, rb, vec_r)
                      : ib.make_fsub_zzz(dst_r, ra, rb, vec_r);
  } else {
    v->op == IVO_FADD ? ib.make_fadd_qqq(dst_r, ra, rb, vec_r)
                      : ib.make_fsub_qqq(dst_r, ra, rb, vec_r);
  }
}

/**
 * Emit instructions for SQADD and SQSUB ops, with special optimizations for
 * the common pattern (which occurs when multiplying by a twiddle factor) of
 * adding (subtracting) the result of two saturating multiplications:
 *
 *  SQADD(SQMUL(a, b), SQMUL(c, d)).
 *
 * Instead of emitting a sqrdmulh, sqrdmlah instruction sequence (a direct
 * translation of what the floating-point equivalent does), which would result
 * in two lots of rounding, this function instead emits:
 *
 *   smull, smlal, sqrshrn (for half-width: 4h, 8b)
 *   smull, smull2, smlal, smlal2, sqrshrn, sqrshrn2 (for full-width: 8h, 16b)
 *
 * The widening multiply operations accumulate into registers twice the element
 * width, which are then rounded and narrowed back to the original width at the
 * end. This approach provides better accuracy than the direct
 * sqrdmulh, sqrdmlah sequence.
 *
 * For input where only one operand is a SQMUL,
 *
 *   e.g., SQADD(SQMUL(a, b), rhs)
 *
 * a simpler sqrdmulh, sqadd sequence is emitted. Note that this could instead
 * be replaced by a sqrdmlah instruction, but that optimization is left for
 * future work (see TODO).
 */
template<bool IsSVE>
static inline void emit_sqadd_sqsub(sloejit::aarch64::instr_builder &ib,
                                    std::vector<rodata_info> &data_ofs,
                                    std::vector<uint8_t> &data_bytes,
                                    regmap_t &regmap, ir_value v) {
  ASSERT(!IsSVE && "SQADD/SQSUB not yet implemented for SVE");

  ASSERT(v->deps.size() == 2);
  auto lhs = v->deps[0];
  auto rhs = v->deps[1];
  ASSERT(lhs->type->nelems == rhs->type->nelems);

  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  ASSERT(v->type->nelems.is_contig());
  auto total_width = v->type->nelems.count_contig() * v->type->elem_width;
  auto elem_width = v->type->elem_width;
  auto qv = get_qv(total_width, elem_width);

  if (lhs->op != IVO_SQMUL || rhs->op != IVO_SQMUL) {
    auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, lhs);
    auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, rhs);
    v->op == IVO_SQADD ? ib.make_sqadd_qqq(dst_r, ra, rb, qv)
                       : ib.make_sqsub_qqq(dst_r, ra, rb, qv);
    return;
  }
  // TODO: can emit sqrdml(a|s)h if only one of lhs/rhs is a SQMUL

  auto needs_hi = total_width > 64;
  auto is_add = v->op == IVO_SQADD;
  auto shift = elem_width - 1; // Q format shift (Q0.7 or Q0.15)
  auto qv_lo = get_qv(64, elem_width);
  auto qv_hi = get_qv(128, elem_width);
  auto qv_wide = get_qv(128, elem_width * 2);

  auto a = lhs->deps[0];
  auto b = lhs->deps[1];
  auto c = rhs->deps[0];
  auto d = rhs->deps[1];

  if (is_dup(a)) {
    std::swap(a, b);
  }

  if (is_dup(c)) {
    std::swap(c, d);
  }

  auto ra = q_(lhs->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rc = q_(rhs->scope, ib, data_ofs, data_bytes, regmap, c);

  sloejit::reg lo;
  sloejit::reg hi;

  // Emit a widening multiply for SQMUL(a, b)
  if (is_dup(b)) {
    ASSERT(b->deps.size() == 1);
    auto dup_arg = b->deps[0];
    auto dup_idx = (int)b->literals[0];
    auto dup_reg = q_(lhs->scope, ib, data_ofs, data_bytes, regmap, dup_arg);

    if (elem_width == 8) {
      // there is no smull (by element) for 8b/16b,
      // so emit dup and smull (vector) instead
      auto rb = ib.make_dup_ql(dup_reg, dup_idx, qv_hi);
      lo = ib.make_smull_qq(ra, rb, qv_wide, qv_lo);
      if (needs_hi) {
        hi = ib.make_smull2_qq(ra, rb, qv_wide, qv_hi);
      }
    } else {
      lo = ib.make_smull_qql(ra, dup_reg, dup_idx, qv_wide, qv_lo);
      if (needs_hi) {
        hi = ib.make_smull2_qql(ra, dup_reg, dup_idx, qv_wide, qv_hi);
      }
    }

    if (lhs->uses.size() == 1) {
      b->erase_use(lhs);
    }
  } else {
    auto rb = q_(lhs->scope, ib, data_ofs, data_bytes, regmap, b);
    lo = ib.make_smull_qq(ra, rb, qv_wide, qv_lo);
    if (needs_hi) {
      hi = ib.make_smull2_qq(ra, rb, qv_wide, qv_hi);
    }
  }

  // Accumulate SQMUL(c, d)
  if (is_dup(d)) {
    auto dup_arg = d->deps[0];
    auto dup_idx = (int)d->literals[0];
    auto dup_reg = q_(rhs->scope, ib, data_ofs, data_bytes, regmap, dup_arg);

    if (elem_width == 8) {
      // there is no sml(a|s)l (by element) for 8b/16b,
      // so emit dup and sml(a|s)l (vector) instead
      auto rd = ib.make_dup_ql(dup_reg, dup_idx, qv_hi);
      is_add ? ib.make_smlal_qqq(lo, rc, rd, qv_wide, qv_lo)
             : ib.make_smlsl_qqq(lo, rc, rd, qv_wide, qv_lo);
      if (needs_hi) {
        is_add ? ib.make_smlal2_qqq(hi, rc, rd, qv_wide, qv_hi)
               : ib.make_smlsl2_qqq(hi, rc, rd, qv_wide, qv_hi);
      }
    } else {
      is_add ? ib.make_smlal_qqql(lo, rc, dup_reg, dup_idx, qv_wide, qv_lo)
             : ib.make_smlsl_qqql(lo, rc, dup_reg, dup_idx, qv_wide, qv_lo);
      if (needs_hi) {
        is_add ? ib.make_smlal2_qqql(hi, rc, dup_reg, dup_idx, qv_wide, qv_hi)
               : ib.make_smlsl2_qqql(hi, rc, dup_reg, dup_idx, qv_wide, qv_hi);
      }
    }

    if (rhs->uses.size() == 1) {
      d->erase_use(rhs);
    }
  } else {
    auto rd = q_(rhs->scope, ib, data_ofs, data_bytes, regmap, d);
    is_add ? ib.make_smlal_qqq(lo, rc, rd, qv_wide, qv_lo)
           : ib.make_smlsl_qqq(lo, rc, rd, qv_wide, qv_lo);
    if (needs_hi) {
      is_add ? ib.make_smlal2_qqq(hi, rc, rd, qv_wide, qv_hi)
             : ib.make_smlsl2_qqq(hi, rc, rd, qv_wide, qv_hi);
    }
  }

  // Narrow the widened result back to the destination vector width.
  if (needs_hi) {
    ib.make_sqrshrn_qqi(dst_r, lo, shift, qv_lo, qv_wide);
    ib.make_sqrshrn2_qqi(dst_r, hi, shift, qv, qv_wide);
  } else {
    ib.make_sqrshrn_qqi(dst_r, lo, shift, qv, qv_wide);
  }

  // Remove the add as a use of the muls
  lhs->erase_use(v);
  rhs->erase_use(v);
}

template<bool IsSVE>
static inline void emit_fmul(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  // TODO: can use smaller NEON/scalar versions if e.g. !a->type->nelems.sve.
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  if (is_dup(a)) {
    std::swap(a, b);
  }

  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto v_variant = get_qzv<IsSVE>(v->type.get());

  if (is_dup(b)) {
    // if one of the arguments to the fmul is a broadcast, then we can
    // use indexed fmul
    ASSERT(b->deps.size() == 1);
    if constexpr (IsSVE) {
      // Placeholder for indexed fmul in case we ever need it
      ASSERT(false && "Indexed FMUL not yet implemented for SVE");
    } else {
      auto dup_arg = b->deps[0];
      auto dup_idx = (int)b->literals[0];
      auto dup_arg_r =
          get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, dup_arg);
      ib.make_fmul_qqql(dst_r, ra, dup_arg_r, dup_idx, v_variant);
    }
    // remove the fmul as a use of the broadcast
    b->erase_use(v);
    return;
  }
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  if constexpr (IsSVE) {
    ib.make_fmul_zzz(dst_r, ra, rb, v_variant);
  } else {
    ib.make_fmul_qqq(dst_r, ra, rb, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_sqmul(sloejit::aarch64::instr_builder &ib,
                              std::vector<rodata_info> &data_ofs,
                              std::vector<uint8_t> &data_bytes,
                              regmap_t &regmap, ir_value v) {
  if constexpr (IsSVE) {
    ASSERT(false && "SQMUL not yet implemented for SVE");
  }
  // TODO: can use smaller NEON versions if e.g. !a->type->nelems.sve.
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  if (is_dup(a)) {
    std::swap(a, b);
  }

  auto dst_r = get_r<false>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto ra = get_r<false>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto qzv = get_qzv<false>(v->type.get());

  if (v->type->elem_width == 8) {
    // Neon lacks sqrdmulh (by element) for 8b/16b, so emit a smull, sqrshrn
    // (and smull2, sqrshrn2 when the vector spans the high half). sequence
    // instead.

    auto shift = 7; // Q0.7
    auto qv_8b = sloejit::aarch64::qv_8b;
    auto qv_8h = sloejit::aarch64::qv_8h;
    auto qv_16b = sloejit::aarch64::qv_16b;

    sloejit::reg rb;
    if (is_dup(b)) {
      // Neon lacks smull (by element) for 8b/16b,
      // so use dup and smull (vector) instead
      ASSERT(b->deps.size() == 1);
      auto dup_arg = b->deps[0];
      auto dup_idx = (int)b->literals[0];
      auto dup_arg_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, dup_arg);
      rb = ib.make_dup_ql(dup_arg_r, dup_idx, qv_16b);
      b->erase_use(v);
    } else {
      rb = q_(v->scope, ib, data_ofs, data_bytes, regmap, b);
    }

    auto needs_hi = qzv == qv_16b;
    auto lo = ib.make_smull_qq(ra, rb, qv_8h, qv_8b);
    if (needs_hi) {
      auto hi = ib.make_smull2_qq(ra, rb, qv_8h, qv_16b);
      ib.make_sqrshrn_qqi(dst_r, lo, shift, qv_8b, qv_8h);
      ib.make_sqrshrn2_qqi(dst_r, hi, shift, qzv, qv_8h);
    } else {
      ib.make_sqrshrn_qqi(dst_r, lo, shift, qzv, qv_8h);
    }
  } else {
    if (is_dup(b)) {
      // if one of the arguments to the sqrdmulh instruction is a broadcast,
      // then we can use an indexed sqrdmulh instruction.
      ASSERT(b->deps.size() == 1);
      auto dup_arg = b->deps[0];
      auto dup_idx = (int)b->literals[0];
      auto dup_arg_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, dup_arg);
      ib.make_sqrdmulh_qqql(dst_r, ra, dup_arg_r, dup_idx, qzv);
      // remove the sqmul as a use of the broadcast
      b->erase_use(v);
    } else {
      auto rb = q_(v->scope, ib, data_ofs, data_bytes, regmap, b);
      ib.make_sqrdmulh_qqq(dst_r, ra, rb, qzv);
    }
  }
}

template<bool IsSVE>
static inline void
emit_fcmla(sloejit::aarch64::instr_builder &ib,
           std::vector<rodata_info> &data_ofs, std::vector<uint8_t> &data_bytes,
           regmap_t &regmap, ir_value v, sloejit::reg *acc = nullptr) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];

  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto vec_r = get_qzv<IsSVE>(v->type.get());
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);

  if constexpr (IsSVE) {
    if (acc) {
      ib.make_orr_zzz(dst_r, *acc, *acc);
    } else {
      z_zero(dst_r, ib);
    }
  } else {
    if (acc) {
      ib.make_orr_qqq(dst_r, *acc, *acc,
                      get_qv_bytes(v->type->elem_width, v->type->nelems));
    } else {
      ib.make_dup_qr(dst_r, sloejit::aarch64::xzr, vec_r);
    }
  }

  // if either operand contain an exactly-zero real/imag component,
  // we can avoid emitting one of the two FCMLA instructions.
  // (0+a1i)*b ==> use FCMLA acc, A, B, #90
  // (a0+0i)*b ==> use FCMLA acc, A, B, #0
  // a*(0+b1i) ==> use FCMLA acc, B, A, #90
  // a*(b0+0i) ==> use FCMLA acc, B, A, #0
  // TODO: propagate that lane 0 is unused when rot is #90, or
  //       propagate that lane 1 is unused when rot is #0,
  //       in case it simplifies things further up
  if ((a->op == IVO_CONCAT && a->deps.size() == 2) ||
      (b->op == IVO_CONCAT && b->deps.size() == 2)) {
    auto concat = a->op == IVO_CONCAT ? a : b;
    auto rn = a->op == IVO_CONCAT ? ra : rb;
    auto rm = a->op == IVO_CONCAT ? rb : ra;
    auto concat0 = concat->deps[0];
    auto concat1 = concat->deps[1];
    if ((concat0->op == IVO_CONST_FLOAT && concat0->literals[0] == 0) ||
        (concat1->op == IVO_CONST_FLOAT && concat1->literals[0] == 0)) {
      int rot = concat0->op == IVO_CONST_FLOAT ? 90 : 0;
      if constexpr (IsSVE) {
        ib.make_fcmla_zpzzi(dst_r, p_true, rn, rm, rot, vec_r);
      } else {
        ib.make_fcmla_qqqi(dst_r, rn, rm, rot, vec_r);
      }
      return;
    }
  }
  if constexpr (IsSVE) {
    ib.make_fcmla_zpzzi(dst_r, p_true, ra, rb, 0, vec_r);
    ib.make_fcmla_zpzzi(dst_r, p_true, ra, rb, 90, vec_r);
  } else {
    ib.make_fcmla_qqqi(dst_r, ra, rb, 0, vec_r);
    ib.make_fcmla_qqqi(dst_r, ra, rb, 90, vec_r);
  }
}

template<bool IsSVE>
static inline void emit_sqcmla(sloejit::aarch64::instr_builder &ib,
                               std::vector<rodata_info> &data_ofs,
                               std::vector<uint8_t> &data_bytes,
                               regmap_t &regmap, ir_value v,
                               sloejit::reg *acc = nullptr) {
  ASSERT(false && "SQRDCMLAH not yet implemented");
}

template<bool IsSVE>
static inline void emit_concat(sloejit::aarch64::instr_builder &ib,
                               std::vector<rodata_info> &data_ofs,
                               std::vector<uint8_t> &data_bytes,
                               regmap_t &regmap, ir_value v) {
  auto elems = v->deps;
  ASSERT(!elems.empty());
  auto elem_t = elems[0]->type;
  ASSERT(is_float_or_fixed_kind(elem_t->kind));
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);

  // TODO: we previously had an optimization where if not all elements of a
  //       concat were used then the concat could be simplified, for example:
  //       {x,y,x,y} can be simplified to just {x,x,x,x} if we know that only
  //       every other lane is accessed by this value's uses.
  //       We should also propagate this to Neon once it exists again!

  // concat with one argument or concat of multiple scalars, but all of them are
  // the same
  if (elems.size() == 1 || is_all_irvalue_equal(elems)) {
    auto ra =
        get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, elems[0]);
    if constexpr (IsSVE) {
      ASSERT(elem_t->nelems.is_contig());
      ASSERT(!elem_t->nelems.is_sve());
      int segment_width = elem_t->nelems.count_contig() * elem_t->elem_width;
      ASSERT(segment_width <= 128);
      auto zv = get_zv(segment_width);
      ib.make_dup_zzl(dst_r, ra, 0, zv);
    } else {
      // TODO: concat of a load could be ld1r.
      ASSERT(v->type->nelems.is_contig());
      ASSERT(!v->type->nelems.is_sve());
      ASSERT(elem_t->nelems.is_contig());
      ASSERT(!elem_t->nelems.is_sve());
      int item_width = elem_t->nelems.count_contig() * elem_t->elem_width;
      int total_width = v->type->nelems.count_contig() * v->type->elem_width;
      while (item_width != total_width / 2) {
        ASSERT(item_width < total_width);
        auto qv = get_qv(item_width * 2, item_width);
        ra = ib.make_zip1_qq(ra, ra, qv);
        item_width *= 2;
      }
      ASSERT(total_width <= 128);
      auto qv = get_qv(total_width, total_width / 2);
      ib.make_zip1_qqq(dst_r, ra, ra, qv);
    }
    return;
  } else if (elems.size() == 2) {
    auto ra =
        get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, elems[0]);
    auto rb =
        get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, elems[1]);
    if constexpr (IsSVE) {
      ASSERT(elem_t->nelems.is_one());
      auto zv_a = get_zv(elem_t->elem_width);
      auto zv_b = get_zv(elem_t->elem_width * 2);
      auto zipped = ib.make_zip1_zz(ra, rb, zv_a);
      ib.make_dup_zzl(dst_r, zipped, 0, zv_b);
    } else {
      ASSERT(v->type->nelems.is_contig());
      ASSERT(!v->type->nelems.is_sve());
      ASSERT(elem_t->nelems.is_contig());
      ASSERT(!elem_t->nelems.is_sve());
      int total_width = v->type->nelems.count_contig() * v->type->elem_width;
      ASSERT(total_width <= 128);
      auto qv = get_qv(total_width, total_width / 2);
      ib.make_zip1_qqq(dst_r, ra, rb, qv);
    }
    return;
  } else if (elems.size() == 4) {
    if constexpr (IsSVE) {
      ASSERT(false && "unimplemented n-ary svdup_n");
    } else {
      ASSERT(elem_t->nelems.is_contig());
      ASSERT(!elem_t->nelems.is_sve());
      int total_width = v->type->nelems.count_contig() * v->type->elem_width;
      ASSERT(total_width <= 128);
      auto qv_a = get_qv(total_width / 2, total_width / 4);
      auto qv_b = get_qv(total_width, total_width / 2);
      auto ra = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[0]);
      auto rb = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[1]);
      auto rc = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[2]);
      auto rd = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[3]);
      auto a = ib.make_zip1_qq(ra, rb, qv_a);
      auto b = ib.make_zip1_qq(rc, rd, qv_a);
      ib.make_zip1_qqq(dst_r, a, b, qv_b);
    }
    return;
  } else if (elems.size() == 8) {
    if constexpr (IsSVE) {
      ASSERT(false && "unimplemented n-ary svdup_n");
    } else {
      ASSERT(elem_t->nelems.is_contig());
      ASSERT(!elem_t->nelems.is_sve());
      int total_width = v->type->nelems.count_contig() * v->type->elem_width;
      ASSERT(total_width <= 128);
      auto qv_pair = get_qv(total_width / 4, total_width / 8);
      auto qv_half = get_qv(total_width / 2, total_width / 4);
      auto qv_full = get_qv(total_width, total_width / 2);
      auto ra = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[0]);
      auto rb = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[1]);
      auto rc = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[2]);
      auto rd = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[3]);
      auto re = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[4]);
      auto rf = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[5]);
      auto rg = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[6]);
      auto rh = q_(v->scope, ib, data_ofs, data_bytes, regmap, elems[7]);
      auto ab = ib.make_zip1_qq(ra, rb, qv_pair);
      auto cd = ib.make_zip1_qq(rc, rd, qv_pair);
      auto ef = ib.make_zip1_qq(re, rf, qv_pair);
      auto gh = ib.make_zip1_qq(rg, rh, qv_pair);
      auto abcd = ib.make_zip1_qq(ab, cd, qv_half);
      auto efgh = ib.make_zip1_qq(ef, gh, qv_half);
      ib.make_zip1_qqq(dst_r, abcd, efgh, qv_full);
    }
    return;
  }
  ASSERT(false && "unimplemented IVO_CONCAT");
}

template<bool IsSVE>
static inline void emit_shuffle(sloejit::aarch64::instr_builder &ib,
                                std::vector<rodata_info> &data_ofs,
                                std::vector<uint8_t> &data_bytes,
                                regmap_t &regmap, ir_value v) {
  ASSERT(v->op == IVO_SHUFFLE);
  ASSERT(v->deps.size() == 1);
  auto a = v->deps[0];
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);

  if (is_rev_pairs(v)) {
    ASSERT(v->type->nelems == a->type->nelems);
    ASSERT(a->type->nelems.is_even());
    if constexpr (IsSVE) {
      if (a->type->elem_width == 64) {
        auto rb = ib.make_trn1_zz(ra, ra, sloejit::aarch64::zv_d);
        ib.make_trn2_zzz(dst_r, ra, rb, sloejit::aarch64::zv_d);
        return;
      } else if (a->type->elem_width == 32 || a->type->elem_width == 16) {
        z_zero(dst_r, ib);
        a->type->elem_width == 32
            ? ib.make_revw_zpz(dst_r, p_true, ra, sloejit::aarch64::zv_d)
            : ib.make_revh_zpz(dst_r, p_true, ra, sloejit::aarch64::zv_s);
        return;
      }
    } else {
      ASSERT(a->type->nelems.is_contig());
      ASSERT(!a->type->nelems.is_sve());
      int total_width = a->type->nelems.count_contig() * a->type->elem_width;
      ASSERT(total_width <= 128);
      if (a->type->elem_width == 64) {
        ASSERT(total_width == 128);
        ib.make_ext_qqq(dst_r, ra, ra, 8, sloejit::aarch64::qv_16b);
      } else if (a->type->elem_width == 32) {
        auto qv = get_qv(total_width, a->type->elem_width);
        ib.make_rev64_qq(dst_r, ra, qv);
      } else if (a->type->elem_width == 16) {
        auto qv = get_qv(total_width, a->type->elem_width);
        ib.make_rev32_qq(dst_r, ra, qv);
      } else if (a->type->elem_width == 8) {
        auto qv = get_qv(total_width, a->type->elem_width);
        ib.make_rev16_qq(dst_r, ra, qv);
      } else {
        ASSERT(false);
      }
      return;
    }
  } else if (is_trn(v)) {
    ASSERT(v->type->nelems == a->type->nelems);
    auto id = (int)v->literals[0];
    ASSERT(id == 0 || id == 1);
    ASSERT(a->type->nelems.is_contig());
    ASSERT(a->type->nelems.is_even());
    int segment_width = a->type->nelems.count_contig() * a->type->elem_width;
    ASSERT(segment_width <= 128);
    auto v_variant = get_qzv<IsSVE>(a->type.get());
    if constexpr (IsSVE) {
      id == 0 ? ib.make_trn1_zzz(dst_r, ra, ra, v_variant)
              : ib.make_trn2_zzz(dst_r, ra, ra, v_variant);
    } else {
      id == 0 ? ib.make_trn1_qqq(dst_r, ra, ra, v_variant)
              : ib.make_trn2_qqq(dst_r, ra, ra, v_variant);
    }
    return;
  } else if (is_neon_take_real(v)) {
    auto v_variant = get_qzv<IsSVE>(a->type.get());
    if constexpr (IsSVE) {
      ib.make_uzp1_zzz(dst_r, ra, ra, v_variant);
    } else {
      ASSERT(a->type->nelems.is_contig());
      ASSERT(a->type->nelems.count_contig() == 4);
      ib.make_uzp1_qqq(dst_r, ra, ra, v_variant);
    }
    return;
  }
  if constexpr (!IsSVE) {
    if (is_rev_vec(v)) {
      ASSERT(a->type->nelems.is_even());
      int elem_width = a->type->elem_width;
      int total_width = a->type->nelems.count_contig() * elem_width;
      if (total_width == 128) {
        if (elem_width == 64) {
          ib.make_ext_qqq(dst_r, ra, ra, 8, sloejit::aarch64::qv_16b);
          return;
        } else if (elem_width == 32 || elem_width == 16 || elem_width == 8) {
          auto qv = get_qv(total_width, elem_width);
          ra = ib.make_rev64_q(ra, qv);
          ib.make_ext_qqq(dst_r, ra, ra, 8, sloejit::aarch64::qv_16b);
          return;
        }
        ASSERT(false && "unhandled");
      } else if (total_width == 64) {
        if (elem_width == 16 || elem_width == 8) {
          auto qv = get_qv(total_width, elem_width);
          ib.make_rev64_qq(dst_r, ra, qv);
          return;
        }
        ASSERT(false && "unhandled");
      }
      ASSERT(false && "unhandled");
    } else if (is_lane_get(v)) {
      ASSERT(a->type->nelems.is_contig());
      ASSERT(!a->type->nelems.is_sve());
      auto lane = (int)v->literals[0];
      ASSERT(lane >= 0);
      ASSERT(lane < a->type->nelems.count_contig());
      ASSERT(is_float_or_fixed_kind(a->type->kind));
      auto ofs = a->type->elem_width / 8 * lane;
      // TODO: this can be an ins/dup rather than an ext, if that matters.
      // TODO: and ext doesn't need to use the whole vector, if the lane is in
      // the first half.
      ib.make_ext_qqq(dst_r, ra, ra, ofs, sloejit::aarch64::qv_16b);
      return;
    } else if (is_neon_take_imag(v)) {
      ASSERT(a->type->nelems.is_contig());
      ASSERT(a->type->nelems.count_contig() == 4);
      ASSERT(!a->type->nelems.is_sve());
      int total_width = 4 * a->type->elem_width;
      auto qv = get_qv(total_width, a->type->elem_width);
      ib.make_uzp2_qqq(dst_r, ra, ra, qv);
      return;
    }
  }

  ASSERT(false && "unimplemented IVO_SHUFFLE");
}

template<bool IsSVE>
static inline void emit_zip1(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  if constexpr (IsSVE) {
    auto v_variant = v->literals.empty() ? get_qzv<true>(v->type.get())
                                         : get_zv((int)v->literals[0]);
    ib.make_zip1_zzz(dst_r, ra, rb, v_variant);
  } else {
    auto v_variant = v->literals.empty() ? get_qzv<false>(v->type.get())
                                         : get_qv(128, (int)v->literals[0]);
    ib.make_zip1_qqq(dst_r, ra, rb, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_zip2(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  ASSERT(v->literals.size() == 1);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  if constexpr (IsSVE) {
    auto v_variant = get_zv((int)v->literals[0]);
    ib.make_zip2_zzz(dst_r, ra, rb, v_variant);
  } else {
    auto v_variant = get_qv(128, (int)v->literals[0]);
    ib.make_zip2_qqq(dst_r, ra, rb, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_trn1(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto v_variant = get_qzv<IsSVE>(v->type.get());
  if constexpr (IsSVE) {
    ib.make_trn1_zzz(dst_r, ra, rb, v_variant);
  } else {
    ib.make_trn1_qqq(dst_r, ra, rb, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_trn2(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  auto v_variant = get_qzv<IsSVE>(v->type.get());
  if constexpr (IsSVE) {
    ib.make_trn2_zzz(dst_r, ra, rb, v_variant);
  } else {
    ib.make_trn2_qqq(dst_r, ra, rb, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_uzp1(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  ASSERT(v->literals.size() == 1);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  if constexpr (IsSVE) {
    auto v_variant = get_zv((int)v->literals[0]);
    ib.make_uzp1_zzz(dst_r, ra, rb, v_variant);
  } else {
    auto v_variant = get_qzv<false>(v->type.get());
    ib.make_uzp1_qqq(dst_r, ra, rb, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_uzp2(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  ASSERT(v->literals.size() == 1);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto rb = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, b);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  if constexpr (IsSVE) {
    auto v_variant = get_zv((int)v->literals[0]);
    ib.make_uzp2_zzz(dst_r, ra, rb, v_variant);
  } else {
    auto v_variant = get_qzv<false>(v->type.get());
    ib.make_uzp2_qqq(dst_r, ra, rb, v_variant);
  }
}

static inline void emit_cnth(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  assert(v->deps.size() == 0);
  auto dst_r = x_(ib, regmap, v);
  ib.make_cnth_r(dst_r);
}

static inline void emit_cntw(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  assert(v->deps.size() == 0);
  auto dst_r = x_(ib, regmap, v);
  ib.make_cntw_r(dst_r);
}

static inline void emit_cntd(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  assert(v->deps.size() == 0);
  auto dst_r = x_(ib, regmap, v);
  ib.make_cntd_r(dst_r);
}

template<bool IsSVE>
static inline sloejit::reg emit_gep(sloejit::aarch64::instr_builder &ib,
                                    std::vector<rodata_info> &data_ofs,
                                    std::vector<uint8_t> &data_bytes,
                                    regmap_t &regmap, ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->nelems.is_one());
  ASSERT(v->type->nelems.is_one() && "Vector GEP not supported");
  auto inner_type = a->type->inner_type;
  ASSERT(!inner_type->nelems.is_sve());
  ASSERT(inner_type->nelems.is_contig());
  int inner_total_width =
      inner_type->nelems.count_contig() * inner_type->elem_width;

  auto dst_r = x_(ib, regmap, v);
  if (b->op == IVO_CONST_INT) {
    // If we can replace
    // movz x0, #n
    // add x2, x1, x0, #lsl m
    // with
    // add x2, x1, #(n << m)
    // We don't need to check that x0 (a) and x1 (b) are the same
    // instruction, as we know that they have a different type, and must be
    // a different instruction (see asserts for {a,b}->type->kind above)
    ASSERT(b->literals.size() == 1);
    int imm = b->literals[0];
    int shift = *get_pow2_lsl_int(inner_total_width / 8);
    if (imm >= 0) {
      ib.make_x_add_rri(dst_r, x_(ib, regmap, a), imm * (1 << shift));
    } else {
      ib.make_x_sub_rri(dst_r, x_(ib, regmap, a), -imm * (1 << shift));
    }
    b->erase_use(v);
    return dst_r;
  }
  auto shift = get_pow2_lsl(inner_total_width / 8);
  ib.make_x_add_rrr(dst_r, x_(ib, regmap, a), x_(ib, regmap, b), shift);
  return dst_r;
}

static inline void emit_gather_zprz(sloejit::aarch64::instr_builder &ib,
                                    std::vector<rodata_info> &data_ofs,
                                    std::vector<uint8_t> &data_bytes,
                                    regmap_t &regmap, sloejit::reg dst_r,
                                    sloejit::reg base_r, sloejit::reg ofs_r,
                                    ir_value v) {
  int mask = v->type->nelems.segment_mask;
  switch (v->type->elem_width) {
  case 64: {
    ASSERT(mask == 0b01 || mask == 0b11);
    if (mask == 0b11) {
      ib.make_ld1d_zprz(dst_r, pred_full(ib, regmap, 64), base_r, ofs_r,
                        sloejit::aarch64::lsl_3, sloejit::aarch64::zv_d);
    } else {
      ofs_r = ib.make_zip1_zz(ofs_r, ofs_r, sloejit::aarch64::zv_d);
      ib.make_ld1d_zprz(dst_r, pred_real(ib, regmap, 64), base_r, ofs_r,
                        sloejit::aarch64::lsl_3, sloejit::aarch64::zv_d);
    }
    break;
  }
  case 32: {
    ASSERT(mask == 0b0101 || mask == 0b1111);
    if (mask == 0b1111) {
      ib.make_ld1d_zprz(dst_r, pred_full(ib, regmap, 32), base_r, ofs_r,
                        sloejit::aarch64::lsl_3, sloejit::aarch64::zv_d);
    } else {
      ib.make_ld1w_zprz(dst_r, pred_real(ib, regmap, 32), base_r, ofs_r,
                        sloejit::aarch64::lsl_2, sloejit::aarch64::zv_d);
    }
    break;
  }
  case 16: {
    ASSERT(mask == 0b01010101 || mask == 0b11111111);
    if (mask == 0b11111111) {
      ib.make_ld1w_zprz(dst_r, pred_full(ib, regmap, 16), base_r, ofs_r,
                        sloejit::aarch64::lsl_2, sloejit::aarch64::zv_s);
    } else {
      ib.make_ld1h_zprz(dst_r, pred_real(ib, regmap, 16), base_r, ofs_r,
                        sloejit::aarch64::lsl_1, sloejit::aarch64::zv_s);
    }
    break;
  }
  default:
    ASSERT(false);
  }
}

static inline bool is_scalar_const_int(ir_value x, int64_t val) {
  return x->op == IVO_CONST_INT && x->type->nelems.is_one() &&
         x->literals.size() == 1 && x->literals[0] == val;
}

static inline bool is_sve_const_index(ir_value v, int64_t base, int64_t step) {
  if (v->op != IVO_INDEX || !v->type->nelems.is_sve()) {
    return false;
  }
  ASSERT(v->deps.size() == 2);
  return is_scalar_const_int(v->deps[0], base) &&
         is_scalar_const_int(v->deps[1], step);
}

static inline bool is_sve_reverse_index(ir_value v) {
  return is_sve_const_index(v, 0, -1);
}

static inline bool is_full_segment_mask(ir_value_type_ptr t) {
  const auto mask = t->nelems.segment_mask;
  return mask != 0 && (mask & (mask + 1)) == 0;
}

static inline sloejit::reg
reverse_contiguous_base(sloejit::aarch64::instr_builder &ib,
                        sloejit::reg base_r, sloejit::reg pred_r,
                        ir_value_type_ptr t) {
  auto active_elems =
      ib.b->fresh_vreg(sloejit::aarch64::x_space, sloejit::aarch64::x_regs);
  const int elem_bits = effective_elem_bits(*t);
  ib.make_cntp_rpp(active_elems, p_true, pred_r, get_zv(elem_bits));

  auto adjusted =
      ib.make_x_sub_rr(base_r, active_elems, get_pow2_lsl(elem_bits / 8));
  return ib.make_x_add_ri(adjusted, effective_elem_bits(*t) / 8);
}

static inline sloejit::reg
reverse_contiguous_pred(sloejit::aarch64::instr_builder &ib,
                        sloejit::reg pred_r, ir_value_type_ptr t) {
  return ib.make_rev_p(pred_r, get_zv(effective_elem_bits(*t)));
}

static inline void emit_load_zpri(sloejit::aarch64::instr_builder &ib,
                                  std::vector<rodata_info> &data_ofs,
                                  std::vector<uint8_t> &data_bytes,
                                  regmap_t &regmap, sloejit::reg dst_r,
                                  sloejit::reg base_r, int ofs,
                                  sloejit::reg pred_r, bool use_pred,
                                  ir_value v) {
  int mask = v->type->nelems.segment_mask;
  switch (v->type->elem_width) {
  case 64: {
    ASSERT(mask == 0b01 || mask == 0b11);
    if (mask == 0b11) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 64);
      ib.make_ld1d_zpri(dst_r, pred, base_r, ofs, sloejit::aarch64::zv_d);
    } else {
      // There is no .d->.q widening load instruction in SVE, so to load into
      // even lanes we need to load half the vector and then interleave it
      // with zero ourselves.
      // TODO: If we know that we are on VL128, the zip does nothing and can be
      //       removed.
      auto ra = ib.make_ld1d_pri(pred_half(ib, regmap, 64), base_r, ofs,
                                 sloejit::aarch64::zv_d);
      ib.make_zip1_zzz(dst_r, ra, z_zero(ib), sloejit::aarch64::zv_d);
    }
    break;
  }
  case 32: {
    ASSERT(mask == 0b0101 || mask == 0b1111);
    if (mask == 0b1111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 32);
      ib.make_ld1d_zpri(dst_r, pred, base_r, ofs, sloejit::aarch64::zv_d);
    } else {
      ib.make_ld1w_zpri(dst_r, pred_real(ib, regmap, 32), base_r, ofs,
                        sloejit::aarch64::zv_d);
    }
    break;
  }
  case 16: {
    ASSERT(mask == 0b01010101 || mask == 0b11111111);
    if (mask == 0b11111111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 16);
      ib.make_ld1w_zpri(dst_r, pred, base_r, ofs, sloejit::aarch64::zv_s);
    } else {
      ib.make_ld1h_zpri(dst_r, pred_real(ib, regmap, 16), base_r, ofs,
                        sloejit::aarch64::zv_s);
    }
    break;
  }
  default:
    ASSERT(false);
  }
}

static inline void emit_structure_load_group(
    sloejit::aarch64::instr_builder &ib, std::vector<rodata_info> &data_ofs,
    std::vector<uint8_t> &data_bytes, regmap_t &regmap, ir_value v) {
  ASSERT(v->deps.size() == 2 || v->deps.size() == 3);
  const bool is_sve = v->deps.size() == 3;
  ASSERT(is_sve == v->type->nelems.is_sve());
  auto base = v->deps[is_sve ? 1 : 0];
  auto ofs = v->deps[is_sve ? 2 : 1];
  auto base_r = x_(ib, regmap, base);
  std::vector<sloejit::reg> output_regs(v->uses.size());
  for (const auto &use : v->uses) {
    ASSERT(use.v->op == IVO_GET_GROUP_OP);
    ASSERT(use.v->deps.size() == 1);
    ASSERT(use.v->deps[0] == v);
    ASSERT(use.v->literals.size() == 1);
    const int index = use.v->literals[0];
    output_regs[index] =
        is_sve ? z_(v->scope, ib, data_ofs, data_bytes, regmap, use.v)
               : q_(v->scope, ib, data_ofs, data_bytes, regmap, use.v);
  }
  const int elem_bits = effective_elem_bits(*v->type);
  if (!is_sve) {
    ASSERT(v->type->nelems.is_contig() && v->type->count_contig_bits() == 128);
    auto addr_r = base_r;
    const int elem_bytes = elem_bits / 8;
    if (ofs->op == IVO_CONST_INT) {
      ASSERT(ofs->literals.size() == 1);
      const int byte_offset = (int)ofs->literals[0] * elem_bytes;
      if (byte_offset > 0) {
        addr_r = ib.make_x_add_ri(base_r, byte_offset);
      } else if (byte_offset < 0) {
        addr_r = ib.make_x_sub_ri(base_r, -byte_offset);
      }
    } else {
      addr_r = ib.make_x_add_rr(base_r, x_(ib, regmap, ofs),
                                get_pow2_lsl(elem_bytes));
    }
    const auto qv = get_qv(128, elem_bits);
    switch (v->uses.size()) {
    case 2:
      ib.make_ld2_rrr(output_regs[0], output_regs[1], addr_r, qv);
      return;
    case 3:
      ib.make_ld3_rrrr(output_regs[0], output_regs[1], output_regs[2], addr_r,
                       qv);
      return;
    case 4:
      ib.make_ld4_rrrrr(output_regs[0], output_regs[1], output_regs[2],
                        output_regs[3], addr_r, qv);
      return;
    }
    ASSERT(false && "Unsupported Neon IVO_STRUCTURE_LOAD_GROUP");
  }

  auto ofs_r = x_(ib, regmap, ofs);
  auto pred = v->deps[0];
  auto pred_r = p_(ib, regmap, pred);
  switch (v->uses.size()) {
  case 2: {
    const auto zt1 = output_regs[0];
    const auto zt2 = output_regs[1];
    switch (elem_bits) {
    case 8:
      ib.make_ld2b_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 16:
      ib.make_ld2h_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 32:
      ib.make_ld2w_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 64:
      ib.make_ld2d_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 128:
      ib.make_ld2q_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    }
  }
  case 3: {
    const auto zt1 = output_regs[0];
    const auto zt2 = output_regs[1];
    const auto zt3 = output_regs[2];
    switch (elem_bits) {
    case 8:
      ib.make_ld3b_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 16:
      ib.make_ld3h_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 32:
      ib.make_ld3w_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 64:
      ib.make_ld3d_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 128:
      ib.make_ld3q_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    }
  }
  case 4: {
    const auto zt1 = output_regs[0];
    const auto zt2 = output_regs[1];
    const auto zt3 = output_regs[2];
    const auto zt4 = output_regs[3];
    switch (elem_bits) {
    case 8:
      ib.make_ld4b_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 16:
      ib.make_ld4h_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 32:
      ib.make_ld4w_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 64:
      ib.make_ld4d_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 128:
      ib.make_ld4q_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    }
  }
  }
  ASSERT(false && "Unsupported IVO_STRUCTURE_LOAD_GROUP");
}

static inline void emit_structure_store_group(
    sloejit::aarch64::instr_builder &ib, std::vector<rodata_info> &data_ofs,
    std::vector<uint8_t> &data_bytes, regmap_t &regmap, ir_value v) {
  ASSERT(v->op == IVO_STRUCTURE_STORE_GROUP);
  ASSERT(v->deps.size() >= 4 && v->deps.size() <= 7);
  const bool is_sve = v->deps[0]->type->kind == IVK_PREDICATE;
  const int values_begin = is_sve ? 1 : 0;
  const int n = (int)v->deps.size() - values_begin - 2;
  ASSERT(n == 2 || n == 3 || n == 4);

  auto base = v->deps[values_begin + n];
  auto ofs = v->deps[values_begin + n + 1];
  auto base_r = x_(ib, regmap, base);
  std::vector<sloejit::reg> value_regs(n);
  for (int i = 0; i < n; ++i) {
    auto value = v->deps[values_begin + i];
    value_regs[i] = is_sve
                        ? z_(v->scope, ib, data_ofs, data_bytes, regmap, value)
                        : q_(v->scope, ib, data_ofs, data_bytes, regmap, value);
  }

  const int elem_bits = effective_elem_bits(*v->deps[values_begin]->type);
  if (!is_sve) {
    ASSERT(v->deps[values_begin]->type->nelems.is_contig() &&
           v->deps[values_begin]->type->count_contig_bits() == 128);
    auto addr_r = base_r;
    const int elem_bytes = elem_bits / 8;
    if (ofs->op == IVO_CONST_INT) {
      ASSERT(ofs->literals.size() == 1);
      const int byte_offset = (int)ofs->literals[0] * elem_bytes;
      if (byte_offset > 0) {
        addr_r = ib.make_x_add_ri(base_r, byte_offset);
      } else if (byte_offset < 0) {
        addr_r = ib.make_x_sub_ri(base_r, -byte_offset);
      }
    } else {
      addr_r = ib.make_x_add_rr(base_r, x_(ib, regmap, ofs),
                                get_pow2_lsl(elem_bytes));
    }
    const auto qv = get_qv(128, elem_bits);
    switch (n) {
    case 2:
      ib.make_st2_rrr(value_regs[0], value_regs[1], addr_r, qv);
      return;
    case 3:
      ib.make_st3_rrrr(value_regs[0], value_regs[1], value_regs[2], addr_r, qv);
      return;
    case 4:
      ib.make_st4_rrrrr(value_regs[0], value_regs[1], value_regs[2],
                        value_regs[3], addr_r, qv);
      return;
    }
    ASSERT(false && "Unsupported Neon IVO_STRUCTURE_STORE_GROUP");
  }

  auto pred_r = p_(ib, regmap, v->deps[0]);
  auto ofs_r = x_(ib, regmap, ofs);
  switch (n) {
  case 2: {
    const auto zt1 = value_regs[0];
    const auto zt2 = value_regs[1];
    switch (elem_bits) {
    case 8:
      ib.make_st2b_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 16:
      ib.make_st2h_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 32:
      ib.make_st2w_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    case 64:
      ib.make_st2d_zzprr(zt1, zt2, pred_r, base_r, ofs_r);
      return;
    }
    break;
  }
  case 3: {
    const auto zt1 = value_regs[0];
    const auto zt2 = value_regs[1];
    const auto zt3 = value_regs[2];
    switch (elem_bits) {
    case 8:
      ib.make_st3b_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 16:
      ib.make_st3h_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 32:
      ib.make_st3w_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    case 64:
      ib.make_st3d_zzzprr(zt1, zt2, zt3, pred_r, base_r, ofs_r);
      return;
    }
    break;
  }
  case 4: {
    const auto zt1 = value_regs[0];
    const auto zt2 = value_regs[1];
    const auto zt3 = value_regs[2];
    const auto zt4 = value_regs[3];
    switch (elem_bits) {
    case 8:
      ib.make_st4b_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 16:
      ib.make_st4h_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 32:
      ib.make_st4w_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    case 64:
      ib.make_st4d_zzzzprr(zt1, zt2, zt3, zt4, pred_r, base_r, ofs_r);
      return;
    }
    break;
  }
  }
  ASSERT(false && "Unsupported IVO_STRUCTURE_STORE_GROUP");
}

static inline sloejit::aarch64::hvopt get_za_hv(int plfft_direction_t) {
  return plfft_direction_t == 0 ? sloejit::aarch64::hvopt_h
                                : sloejit::aarch64::hvopt_v;
}

static inline sloejit::reg x12_15_slice(sloejit::aarch64::instr_builder &ib,
                                        regmap_t &regmap, ir_value slice) {
  auto dst = sloejit::aarch64::x12;
  if (slice->op == IVO_CONST_INT) {
    ASSERT(slice->literals.size() == 1);
    int val = (int)slice->literals[0];
    ASSERT(val >= -65536 && val < 65536);
    if (val < 0) {
      ib.make_x_movn_ri(dst, ~val);
    } else {
      ib.make_x_movz_ri(dst, val);
    }
    return dst;
  }
  auto src = x_(ib, regmap, slice);
  ib.make_x_mov_rr(dst, src);
  return dst;
}

static inline void emit_za_slice_write(sloejit::aarch64::instr_builder &ib,
                                       std::vector<rodata_info> &data_ofs,
                                       std::vector<uint8_t> &data_bytes,
                                       regmap_t &regmap, ir_value v) {
  ASSERT(v->deps.size() == 4);
  auto pred = v->deps[1];
  auto vec = v->deps[2];
  auto slice = v->deps[3];
  ASSERT(vec->type->nelems.is_sve());
  ASSERT(slice->type->kind == IVK_INTEGER);
  ASSERT(slice->type->nelems.is_one());
  ASSERT(pred->type->kind == IVK_PREDICATE);
  ASSERT(v->literals.size() == 2);
  int plfft_direction_t = (int)v->literals[0];
  int tile = (int)v->literals[1];
  ASSERT(plfft_direction_t == 0 || plfft_direction_t == 1);

  auto hv = get_za_hv(plfft_direction_t);
  auto slice_r = x12_15_slice(ib, regmap, slice);
  auto vec_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, vec);

  int elem_bits = effective_elem_bits(*vec->type);
  auto pg = p_(ib, regmap, pred);

  auto zv = get_zv(elem_bits);
  ib.make_mova_alorlpz(sloejit::aarch64::za, tile, hv, slice_r, 0, pg, vec_r,
                       zv);
}

static inline void emit_za_slice_read(sloejit::aarch64::instr_builder &ib,
                                      std::vector<rodata_info> &data_ofs,
                                      std::vector<uint8_t> &data_bytes,
                                      regmap_t &regmap, ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto slice = v->deps[1];
  ASSERT(v->type->nelems.is_sve());
  ASSERT(slice->type->kind == IVK_INTEGER);
  ASSERT(slice->type->nelems.is_one());
  ASSERT(v->literals.size() == 2);
  int plfft_direction_t = (int)v->literals[0];
  int tile = (int)v->literals[1];
  ASSERT(plfft_direction_t == 0 || plfft_direction_t == 1);

  auto hv = get_za_hv(plfft_direction_t);
  auto slice_r = x12_15_slice(ib, regmap, slice);
  auto dst_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, v);

  int elem_bits = effective_elem_bits(*v->type);
  auto pg = pred_full(ib, regmap, elem_bits);

  auto zv = get_zv(elem_bits);
  ib.make_mova_zpalorl(dst_r, pg, sloejit::aarch64::za, tile, hv, slice_r, 0,
                       zv);
}

static inline void emit_load_zprr(sloejit::aarch64::instr_builder &ib,
                                  std::vector<rodata_info> &data_ofs,
                                  std::vector<uint8_t> &data_bytes,
                                  regmap_t &regmap, sloejit::reg dst_r,
                                  sloejit::reg base_r, sloejit::reg ofs_r,
                                  sloejit::reg pred_r, bool use_pred,
                                  ir_value v) {
  int mask = v->type->nelems.segment_mask;
  switch (v->type->elem_width) {
  case 64: {
    ASSERT(mask == 0b01 || mask == 0b11);
    if (mask == 0b11) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 64);
      ib.make_ld1d_zprr(dst_r, pred, base_r, ofs_r, sloejit::aarch64::zv_d);
    } else {
      auto ra = ib.make_ld1d_prr(pred_half(ib, regmap, 64), base_r, ofs_r,
                                 sloejit::aarch64::zv_d);
      ib.make_zip1_zzz(dst_r, ra, z_zero(ib), sloejit::aarch64::zv_d);
    }
    break;
  }
  case 32: {
    ASSERT(mask == 0b0101 || mask == 0b1111);
    if (mask == 0b1111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 32);
      ib.make_ld1d_zprr(dst_r, pred, base_r, ofs_r, sloejit::aarch64::zv_d);
    } else {
      ib.make_ld1w_zprr(dst_r, pred_real(ib, regmap, 32), base_r, ofs_r,
                        sloejit::aarch64::zv_d);
    }
    break;
  }
  case 16: {
    ASSERT(mask == 0b01010101 || mask == 0b11111111);
    if (mask == 0b11111111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 16);
      ib.make_ld1w_zprr(dst_r, pred, base_r, ofs_r, sloejit::aarch64::zv_s);
    } else {
      ib.make_ld1h_zprr(dst_r, pred_real(ib, regmap, 16), base_r, ofs_r,
                        sloejit::aarch64::zv_s);
    }
    break;
  }
  default:
    ASSERT(false);
  }
}

static inline void emit_load_rrr(sloejit::aarch64::instr_builder &ib,
                                 std::vector<rodata_info> &data_ofs,
                                 std::vector<uint8_t> &data_bytes,
                                 regmap_t &regmap, sloejit::reg base_r,
                                 sloejit::reg ofs_r, ir_value v) {
  ASSERT(v->type->nelems.is_contig());
  int total_width = v->type->nelems.count_contig() * v->type->elem_width;
  if (total_width == 8) {
    auto dst_r = b_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_b_ldr_rrr(dst_r, base_r, ofs_r);
  } else if (total_width == 16) {
    auto dst_r = h_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_h_ldr_rrr(dst_r, base_r, ofs_r);
  } else if (total_width == 32) {
    auto dst_r = s_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_s_ldr_rrr(dst_r, base_r, ofs_r);
  } else if (total_width == 64) {
    auto dst_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_d_ldr_rrr(dst_r, base_r, ofs_r);
  } else if (total_width == 128) {
    auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_q_ldr_rrr(dst_r, base_r, ofs_r);
  } else {
    ASSERT(false);
  }
}

static inline void emit_load_rri(sloejit::aarch64::instr_builder &ib,
                                 std::vector<rodata_info> &data_ofs,
                                 std::vector<uint8_t> &data_bytes,
                                 regmap_t &regmap, sloejit::reg base_r, int ofs,
                                 ir_value v) {
  ASSERT(v->type->nelems.is_contig());
  int total_width = v->type->nelems.count_contig() * v->type->elem_width;
  if (total_width == 8) {
    auto dst_r = b_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_b_ldr_rri(dst_r, base_r, ofs);
  } else if (total_width == 16) {
    auto dst_r = h_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_h_ldr_rri(dst_r, base_r, ofs);
  } else if (total_width == 32) {
    auto dst_r = s_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_s_ldr_rri(dst_r, base_r, ofs);
  } else if (total_width == 64) {
    auto dst_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_d_ldr_rri(dst_r, base_r, ofs);
  } else if (total_width == 128) {
    auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    ib.make_q_ldr_rri(dst_r, base_r, ofs);
  } else {
    ASSERT(false);
  }
}

static inline sloejit::reg
maybe_add_byte_offset(sloejit::aarch64::instr_builder &ib, sloejit::reg base_r,
                      int byte_offset) {
  if (byte_offset > 0) {
    return ib.make_x_add_ri(base_r, byte_offset);
  }
  if (byte_offset < 0) {
    return ib.make_x_sub_ri(base_r, -byte_offset);
  }
  return base_r;
}

static inline sloejit::reg resolve_base_reg(sloejit::aarch64::instr_builder &ib,
                                            std::vector<rodata_info> &data_ofs,
                                            std::vector<uint8_t> &data_bytes,
                                            regmap_t &regmap, ir_value base) {
  return base->op == IVO_GEP && want_delayed_gep(base)
             ? emit_gep<false>(ib, data_ofs, data_bytes, regmap, base)
             : x_(ib, regmap, base);
}

static inline sloejit::reg
add_scaled_offset_reg(sloejit::aarch64::instr_builder &ib, regmap_t &regmap,
                      sloejit::reg base_r, ir_value ofs, int elem_bytes) {
  auto ofs_r = x_(ib, regmap, ofs);
  auto shift_r = get_pow2_lsl(elem_bytes);
  return ib.make_x_add_rr(base_r, ofs_r, shift_r);
}

template<bool IsSVE>
static inline void emit_load_bcast(sloejit::aarch64::instr_builder &ib,
                                   std::vector<rodata_info> &data_ofs,
                                   std::vector<uint8_t> &data_bytes,
                                   regmap_t &regmap, ir_value v) {
  ASSERT(v->literals.size() == 1);
  auto base = v->deps[0];
  auto ofs = v->deps[1];
  ASSERT(base->type->kind == IVK_POINTER);
  ASSERT(ofs->type->kind == IVK_INTEGER);
  ASSERT(ofs->type->nelems.is_one());
  const int segment_width = v->literals[0];

  if constexpr (IsSVE) {
    ASSERT(v->type->nelems.is_sve());
    auto dst_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    auto base_r = x_(ib, regmap, base);
    auto pred_r = p_true;

    ASSERT(ofs->op == IVO_CONST_INT &&
           "non-constant offset not currently supported by  IVO_LOAD_BCAST");
    const int elem_bytes = base->type->inner_type->count_contig_bytes();
    int ofs_bytes = ofs->literals[0] * elem_bytes;
    int imm_min = 0;
    int imm_max = 0;
    int align = 0;
    if (segment_width == 128) {
      // 128 uses LD1RQD, which has different immediate constraints to LD1R<HWD>
      imm_min = -128;
      imm_max = 112;
      align = 16;
    } else {
      const int shift = *get_pow2_lsl_int(segment_width / 8);
      const int imm_bits = 6 + shift;
      imm_min = -(1 << (imm_bits - 1));
      imm_max = (1 << (imm_bits - 1)) - (1 << shift);
      align = 1 << shift;
    }
    const bool imm_ok =
        ofs_bytes % align == 0 && ofs_bytes >= imm_min && ofs_bytes <= imm_max;
    if (!imm_ok) {
      base_r = add_scaled_offset_reg(ib, regmap, base_r, ofs, elem_bytes);
      ofs_bytes = 0;
    }

    switch (segment_width) {
    case 16:
      ib.make_ld1rh_zpri(dst_r, pred_r, base_r, ofs_bytes);
      break;
    case 32:
      ib.make_ld1rw_zpri(dst_r, pred_r, base_r, ofs_bytes);
      break;
    case 64:
      ib.make_ld1rd_zpri(dst_r, pred_r, base_r, ofs_bytes);
      break;
    case 128:
      ib.make_ld1rqd_zpri(dst_r, pred_r, base_r, ofs_bytes);
      break;
    default:
      ASSERT(false);
    }
  } else {
    ASSERT(!v->type->nelems.is_sve());
    auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    auto base_r = resolve_base_reg(ib, data_ofs, data_bytes, regmap, base);
    const int elem_bytes = base->type->inner_type->count_contig_bytes();

    if (ofs->op == IVO_CONST_INT) {
      const int ofs_bytes = ofs->literals[0] * elem_bytes;
      if (segment_width == 128) {
        emit_load_rri(ib, data_ofs, data_bytes, regmap, base_r, ofs_bytes, v);
        return;
      }
      base_r = maybe_add_byte_offset(ib, base_r, ofs_bytes);
    } else {
      base_r = add_scaled_offset_reg(ib, regmap, base_r, ofs, elem_bytes);
      if (segment_width == 128) {
        emit_load_rri(ib, data_ofs, data_bytes, regmap, base_r, 0, v);
        return;
      }
    }
    const auto qv = get_qv(128, segment_width);
    ib.make_ld1r_rr(dst_r, base_r, qv);
    return;
  }
}

template<bool IsSVE, bool IsSME>
static inline void emit_gather(sloejit::aarch64::instr_builder &ib,
                               std::vector<rodata_info> &data_ofs,
                               std::vector<uint8_t> &data_bytes,
                               regmap_t &regmap, ir_value v) {
  ASSERT(v->op == IVO_GATHER);
  ASSERT(v->deps.size() == 2);
  ASSERT(is_float_or_fixed_kind(v->type->kind));
  // a[b]
  auto a = v->deps[0];
  auto b = v->deps[1];

  ASSERT(a->type->nelems.is_one());
  ASSERT(!b->type->nelems.is_one());

  if constexpr (IsSVE) {
    auto dst_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    auto base_r = x_(ib, regmap, a);
    if constexpr (IsSME) {
      ASSERT(is_sve_reverse_index(b) &&
             "Cannot emit this access pattern for SME");
      auto pred_r = pred_full(ib, regmap, v->type->elem_width);
      auto rev_pred_r = reverse_contiguous_pred(ib, pred_r, v->type);
      auto load_base_r = reverse_contiguous_base(ib, base_r, pred_r, v->type);
      auto tmp_r =
          ib.b->fresh_vreg(sloejit::aarch64::v_space, sloejit::aarch64::z_regs);
      emit_load_zpri(ib, data_ofs, data_bytes, regmap, tmp_r, load_base_r, 0,
                     pred_r, true, v);
      ib.make_rev_zz(dst_r, tmp_r, get_zv(effective_elem_bits(*v->type)));
      // Splice dst_r with itself - the second half of the vector will get
      // ignored anyway
      ib.make_splice_zpz(dst_r, rev_pred_r, dst_r,
                         get_zv(effective_elem_bits(*v->type)));
      return;
    }
    auto ofs_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, b);
    emit_gather_zprz(ib, data_ofs, data_bytes, regmap, dst_r, base_r, ofs_r, v);
  } else {
    auto base_r = resolve_base_reg(ib, data_ofs, data_bytes, regmap, a);
    ASSERT(v->type->nelems.is_contig());
    int total_width = v->type->nelems.count_contig() * v->type->elem_width;
    ASSERT(b->type->nelems.is_contig());
    int b_count = b->type->nelems.count_contig();
    if (b->op == IVO_INDEX && b_count == 8) {
      // emulate a gather load with eight ldrs and a cascade of zips.
      ASSERT(b->deps.size() == 2);
      auto b_base = b->deps[0];
      auto b_step = b->deps[1];
      ASSERT(b_base->op == IVO_CONST_INT && (int)b_base->literals[0] == 0);
      // load addresses are: {base, base+step, base+2*step, ..., base+7*step}
      auto ofs_r = x_(ib, regmap, b_step);

      switch (total_width) {
      case 64: {
        auto dst_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        // load x8
        auto b0 = ib.make_b_ldr_ri(base_r, 0);
        auto b1 = ib.make_b_ldr_rr(base_r, ofs_r);
        auto idx2_r = ib.make_lsl_ri(ofs_r, 1);
        auto b2 = ib.make_b_ldr_rr(base_r, idx2_r);
        auto idx3_r = ib.make_x_add_rr(ofs_r, ofs_r, sloejit::aarch64::lsl_1);
        auto b3 = ib.make_b_ldr_rr(base_r, idx3_r);
        auto idx4_r = ib.make_lsl_ri(ofs_r, 2);
        auto b4 = ib.make_b_ldr_rr(base_r, idx4_r);
        auto idx5_r = ib.make_x_add_rr(idx4_r, ofs_r, sloejit::aarch64::lsl_0);
        auto b5 = ib.make_b_ldr_rr(base_r, idx5_r);
        auto idx6_r = ib.make_x_add_rr(idx4_r, ofs_r, sloejit::aarch64::lsl_1);
        auto b6 = ib.make_b_ldr_rr(base_r, idx6_r);
        auto idx7_r = ib.make_x_add_rr(idx4_r, idx3_r, sloejit::aarch64::lsl_0);
        auto b7 = ib.make_b_ldr_rr(base_r, idx7_r);
        // zip x7
        auto h01 = ib.make_zip1_qq(b0, b1, sloejit::aarch64::qv_8b);
        auto h23 = ib.make_zip1_qq(b2, b3, sloejit::aarch64::qv_8b);
        auto h45 = ib.make_zip1_qq(b4, b5, sloejit::aarch64::qv_8b);
        auto h67 = ib.make_zip1_qq(b6, b7, sloejit::aarch64::qv_8b);
        auto s0123 = ib.make_zip1_qq(h01, h23, sloejit::aarch64::qv_4h);
        auto s4567 = ib.make_zip1_qq(h45, h67, sloejit::aarch64::qv_4h);
        ib.make_zip1_qqq(dst_r, s0123, s4567, sloejit::aarch64::qv_2s);
        return;
      }
      case 128: {
        auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        // load x8
        auto h0 = ib.make_h_ldr_ri(base_r, 0);
        auto h1 = ib.make_h_ldr_rr(base_r, ofs_r);
        auto idx2_r = ib.make_lsl_ri(ofs_r, 1);
        auto h2 = ib.make_h_ldr_rr(base_r, idx2_r);
        auto idx3_r = ib.make_x_add_rr(ofs_r, ofs_r, sloejit::aarch64::lsl_1);
        auto h3 = ib.make_h_ldr_rr(base_r, idx3_r);
        auto idx4_r = ib.make_lsl_ri(ofs_r, 2);
        auto h4 = ib.make_h_ldr_rr(base_r, idx4_r);
        auto idx5_r = ib.make_x_add_rr(idx4_r, ofs_r, sloejit::aarch64::lsl_0);
        auto h5 = ib.make_h_ldr_rr(base_r, idx5_r);
        auto idx6_r = ib.make_x_add_rr(idx4_r, ofs_r, sloejit::aarch64::lsl_1);
        auto h6 = ib.make_h_ldr_rr(base_r, idx6_r);
        auto idx7_r = ib.make_x_add_rr(idx4_r, idx3_r, sloejit::aarch64::lsl_0);
        auto h7 = ib.make_h_ldr_rr(base_r, idx7_r);
        // zip x7
        auto s01 = ib.make_zip1_qq(h0, h1, sloejit::aarch64::qv_4h);
        auto s23 = ib.make_zip1_qq(h2, h3, sloejit::aarch64::qv_4h);
        auto s45 = ib.make_zip1_qq(h4, h5, sloejit::aarch64::qv_4h);
        auto s67 = ib.make_zip1_qq(h6, h7, sloejit::aarch64::qv_4h);
        auto d0123 = ib.make_zip1_qq(s01, s23, sloejit::aarch64::qv_2s);
        auto d4567 = ib.make_zip1_qq(s45, s67, sloejit::aarch64::qv_2s);
        ib.make_zip1_qqq(dst_r, d0123, d4567, sloejit::aarch64::qv_2d);
        return;
      }
      }
      ASSERT(false);
    }
    if (b->op == IVO_INDEX && b_count == 4) {
      // emulate a gather load with a quad of ldrs and three zips.
      // TODO: it may be more efficient to use a lane-indexed ld1 on machines
      //       where it performs well, however this it not universally true so
      //       we ignore it for now.
      ASSERT(b->deps.size() == 2);
      auto b_base = b->deps[0];
      auto b_step = b->deps[1];
      // Allow a non-zero constant base by folding it into the address.
      if (b_base->op == IVO_CONST_INT) {
        int base_elems = (int)b_base->literals[0];
        int base_bytes = base_elems * (total_width / 8);
        base_r = maybe_add_byte_offset(ib, base_r, base_bytes);
      } else {
        ASSERT(false &&
               "IVO_INDEX base must be const for NEON gather emulation");
      }
      // load addresses are: {base, base+step, base+2*step, base+3*step}
      auto ofs_r = x_(ib, regmap, b_step);
      ASSERT(total_width == 64 || total_width == 128);
      switch (total_width) {
      case 64: {
        auto dst_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        // load x4
        auto h0 = ib.make_h_ldr_ri(base_r, 0);
        auto h1 = ib.make_h_ldr_rr(base_r, ofs_r);
        auto idx2_r = ib.make_lsl_ri(ofs_r, 1);
        auto h2 = ib.make_h_ldr_rr(base_r, idx2_r);
        auto idx3_r = ib.make_x_add_rr(ofs_r, ofs_r, sloejit::aarch64::lsl_1);
        auto h3 = ib.make_h_ldr_rr(base_r, idx3_r);
        // zip x3
        auto s01 = ib.make_zip1_qq(h0, h1, sloejit::aarch64::qv_4h);
        auto s23 = ib.make_zip1_qq(h2, h3, sloejit::aarch64::qv_4h);
        ib.make_zip1_qqq(dst_r, s01, s23, sloejit::aarch64::qv_2s);
        return;
      }
      case 128: {
        auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        // load x4
        auto s0 = ib.make_s_ldr_ri(base_r, 0);
        auto s1 = ib.make_s_ldr_rr(base_r, ofs_r);
        auto idx2_r = ib.make_lsl_ri(ofs_r, 1);
        auto s2 = ib.make_s_ldr_rr(base_r, idx2_r);
        auto idx3_r = ib.make_x_add_rr(ofs_r, ofs_r, sloejit::aarch64::lsl_1);
        auto s3 = ib.make_s_ldr_rr(base_r, idx3_r);
        // zip x3
        auto d01 = ib.make_zip1_qq(s0, s1, sloejit::aarch64::qv_2s);
        auto d23 = ib.make_zip1_qq(s2, s3, sloejit::aarch64::qv_2s);
        ib.make_zip1_qqq(dst_r, d01, d23, sloejit::aarch64::qv_2d);
        return;
      }
      }
      ASSERT(false);
    }
    if (b->op == IVO_INDEX && b_count == 2) {
      // emulate a gather load with a pair of ldrs and a zip.
      // TODO: it may be more efficient to use a lane-indexed ld1 on machines
      //       where it performs well, however this it not universally true so
      //       we ignore it for now.
      ASSERT(b->deps.size() == 2);
      auto b_base = b->deps[0];
      auto b_step = b->deps[1];
      // Allow a non-zero constant base by folding it into the address.
      if (b_base->op == IVO_CONST_INT) {
        int base_elems = (int)b_base->literals[0];
        int base_bytes = base_elems * (total_width / 8);
        base_r = maybe_add_byte_offset(ib, base_r, base_bytes);
      } else {
        ASSERT(false &&
               "IVO_INDEX base must be const for NEON gather emulation");
      }
      auto ofs_r = x_(ib, regmap, b_step);
      switch (total_width) {
      case 32: {
        auto dst_r = s_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        auto ra = ib.make_h_ldr_ri(base_r, 0);
        auto rb = ib.make_h_ldr_rr(base_r, ofs_r);
        ib.make_zip1_qqq(dst_r, ra, rb, sloejit::aarch64::qv_4h);
        return;
      }
      case 64: {
        auto dst_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        auto ra = ib.make_s_ldr_ri(base_r, 0);
        auto rb = ib.make_s_ldr_rr(base_r, ofs_r);
        ib.make_zip1_qqq(dst_r, ra, rb, sloejit::aarch64::qv_2s);
        return;
      }
      case 128: {
        auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
        auto ra = ib.make_d_ldr_ri(base_r, 0);
        auto rb = ib.make_d_ldr_rr(base_r, ofs_r);
        ib.make_zip1_qqq(dst_r, ra, rb, sloejit::aarch64::qv_2d);
        return;
      }
      }
      ASSERT(false);
    }
  }
}

template<bool IsSVE>
static inline void emit_load(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  bool has_pred =
      v->deps.size() == 3 && v->deps[0]->type->kind == IVK_PREDICATE;
  ASSERT(v->op == IVO_LOAD);
  ASSERT(v->deps.size() == 2 || has_pred);
  ASSERT(is_float_or_fixed_kind(v->type->kind));
  auto a = has_pred ? v->deps[1] : v->deps[0];
  auto b = has_pred ? v->deps[2] : v->deps[1];
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->nelems.is_one());

  if constexpr (IsSVE) {
    auto dst_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    auto base_r = x_(ib, regmap, a);
    auto pred_r = p_true;
    bool use_pred = false;
    if (has_pred) {
      auto pred = v->deps[0];
      pred_r = p_(ib, regmap, pred);
      use_pred = true;
    }

    // vector load, scalar base, zero offset
    if (v->type->nelems.is_sve() && is_constant_int_zero(b)) {
      emit_load_zpri(ib, data_ofs, data_bytes, regmap, dst_r, base_r, 0, pred_r,
                     use_pred, v);
      return;
    }

    // vector load, scalar base/offset
    if (v->type->nelems.is_sve()) {
      auto ofs_r = x_(ib, regmap, b);
      emit_load_zprr(ib, data_ofs, data_bytes, regmap, dst_r, base_r, ofs_r,
                     pred_r, use_pred, v);
      return;
    }

    // neon load, scalar base/offset
    auto ofs_r = x_(ib, regmap, b);
    emit_load_rrr(ib, data_ofs, data_bytes, regmap, base_r, ofs_r, v);

  } else {
    auto base_r = resolve_base_reg(ib, data_ofs, data_bytes, regmap, a);
    ASSERT(v->type->nelems.is_contig());
    int total_width = v->type->nelems.count_contig() * v->type->elem_width;
    if (v->type->nelems == a->type->inner_type->nelems) {
      // normal scalar load
      // try to replace a sequence such as:
      // movz x0, n; ldr dst, [x1, x0, lsl #sh]
      // with
      // ldr dst, [x1, #(n << sh)]
      // We assume a zero shift, so that we don't need to worry about that
      // Check whether b is a constant integer value
      if (b->op == IVO_CONST_INT) {
        ASSERT(b->literals.size() == 1);
        auto imm = b->literals[0];
        auto ofs = (int)(imm * total_width / 8);
        if (ofs >= 0) {
          emit_load_rri(ib, data_ofs, data_bytes, regmap, base_r, ofs, v);
          // Remove the current operation as a use of the movz
          b->erase_use(v);
          return;
        }
      }
      // emit ldr dst, [x1, x0, lsl #0] for the appropriate data type
      auto ofs_r = x_(ib, regmap, b);
      emit_load_rrr(ib, data_ofs, data_bytes, regmap, base_r, ofs_r, v);
      return;
    }
    // wider than normal load, if e.g. loop unrolling has happened
    ASSERT(b->op == IVO_CONST_INT);
    ASSERT(a->type->inner_type);
    auto inner_type = a->type->inner_type;
    ASSERT(inner_type->nelems.is_contig());
    int inner_total_width =
        inner_type->nelems.count_contig() * inner_type->elem_width;
    int ofs = ((int)b->literals[0]) << get_pow2_lsl(inner_total_width / 8);
    base_r = maybe_add_byte_offset(ib, base_r, ofs);
    emit_load_rri(ib, data_ofs, data_bytes, regmap, base_r, 0, v);
  }
}

static inline void emit_scatter_zprz(sloejit::aarch64::instr_builder &ib,
                                     std::vector<rodata_info> &data_ofs,
                                     std::vector<uint8_t> &data_bytes,
                                     regmap_t &regmap, sloejit::reg val_r,
                                     sloejit::reg base_r, sloejit::reg ofs_r,
                                     sloejit::reg pred_r, bool use_pred,
                                     ir_value v) {
  int mask = v->type->nelems.segment_mask;
  switch (v->type->elem_width) {
  case 64: {
    ASSERT(mask == 0b01 || mask == 0b11);
    if (mask == 0b11) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 64);
      ib.make_st1d_zprz(val_r, pred, base_r, ofs_r, sloejit::aarch64::lsl_3,
                        sloejit::aarch64::zv_d);
    } else {
      ofs_r = ib.make_zip1_zz(ofs_r, ofs_r, sloejit::aarch64::zv_d);
      ib.make_st1d_zprz(val_r, pred_real(ib, regmap, 64), base_r, ofs_r,
                        sloejit::aarch64::lsl_3, sloejit::aarch64::zv_d);
    }
    break;
  }
  case 32: {
    ASSERT(mask == 0b0101 || mask == 0b1111);
    if (mask == 0b1111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 32);
      ib.make_st1d_zprz(val_r, pred, base_r, ofs_r, sloejit::aarch64::lsl_3,
                        sloejit::aarch64::zv_d);
    } else {
      ib.make_st1w_zprz(val_r, pred_real(ib, regmap, 32), base_r, ofs_r,
                        sloejit::aarch64::lsl_2, sloejit::aarch64::zv_d);
    }
    break;
  }
  case 16: {
    ASSERT(mask == 0b01010101 || mask == 0b11111111);
    if (mask == 0b11111111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 16);
      ib.make_st1w_zprz(val_r, pred, base_r, ofs_r, sloejit::aarch64::lsl_2,
                        sloejit::aarch64::zv_s);
    } else {
      ib.make_st1h_zprz(val_r, pred_real(ib, regmap, 16), base_r, ofs_r,
                        sloejit::aarch64::lsl_1, sloejit::aarch64::zv_s);
    }
    break;
  }
  default:
    ASSERT(false);
  }
}

static inline void emit_scatter_zprr(sloejit::aarch64::instr_builder &ib,
                                     std::vector<rodata_info> &data_ofs,
                                     std::vector<uint8_t> &data_bytes,
                                     regmap_t &regmap, sloejit::reg val_r,
                                     sloejit::reg base_r, sloejit::reg ofs_r,
                                     sloejit::reg pred_r, bool use_pred,
                                     ir_value v) {
  int mask = v->type->nelems.segment_mask;
  switch (v->type->elem_width) {
  case 64: {
    ASSERT(mask == 0b01 || mask == 0b11);
    if (mask == 0b11) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 64);
      ib.make_st1d_zprr(val_r, pred, base_r, ofs_r, sloejit::aarch64::zv_d);
    } else {
      // There is no .q->.d truncating store instruction in SVE, so to store
      // from even lanes we need to load half the vector and then de-interleave
      // to obtain just the lanes to store contiguously.
      // TODO: If we know that we are on VL128, the uzp does nothing and can be
      //       removed.
      val_r = ib.make_uzp1_zz(val_r, val_r, sloejit::aarch64::zv_d);
      ib.make_st1d_zprr(val_r, pred_half(ib, regmap, 64), base_r, ofs_r,
                        sloejit::aarch64::zv_d);
    }
    break;
  }
  case 32: {
    ASSERT(mask == 0b0101 || mask == 0b1111);
    if (mask == 0b1111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 32);
      ib.make_st1d_zprr(val_r, pred, base_r, ofs_r, sloejit::aarch64::zv_d);
    } else {
      ib.make_st1w_zprr(val_r, pred_full(ib, regmap, 32), base_r, ofs_r,
                        sloejit::aarch64::zv_d);
    }
    break;
  }
  case 16: {
    ASSERT(mask == 0b01010101 || mask == 0b11111111);
    if (mask == 0b11111111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 16);
      ib.make_st1w_zprr(val_r, pred, base_r, ofs_r, sloejit::aarch64::zv_s);
    } else {
      ib.make_st1h_zprr(val_r, pred_full(ib, regmap, 16), base_r, ofs_r,
                        sloejit::aarch64::zv_s);
    }
    break;
  }
  default:
    ASSERT(false);
  }
}

static inline void emit_scatter_zpri(sloejit::aarch64::instr_builder &ib,
                                     std::vector<rodata_info> &data_ofs,
                                     std::vector<uint8_t> &data_bytes,
                                     regmap_t &regmap, sloejit::reg val_r,
                                     sloejit::reg base_r, int ofs,
                                     sloejit::reg pred_r, bool use_pred,
                                     ir_value v) {
  int mask = v->type->nelems.segment_mask;
  switch (v->type->elem_width) {
  case 64: {
    ASSERT(mask == 0b01 || mask == 0b11);
    if (mask == 0b11) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 64);
      ib.make_st1d_zpri(val_r, pred, base_r, ofs, sloejit::aarch64::zv_d);
    } else {
      // There is no .q->.d truncating store instruction in SVE, so to store
      // from even lanes we need to load half the vector and then de-interleave
      // to obtain just the lanes to store contiguously.
      // TODO: If we know that we are on VL128, the uzp does nothing and can be
      //       removed.
      val_r = ib.make_uzp1_zz(val_r, val_r, sloejit::aarch64::zv_d);
      ib.make_st1d_zpri(val_r, pred_half(ib, regmap, 64), base_r, ofs,
                        sloejit::aarch64::zv_d);
    }
    break;
  }
  case 32: {
    ASSERT(mask == 0b0101 || mask == 0b1111);
    if (mask == 0b1111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 32);
      ib.make_st1d_zpri(val_r, pred, base_r, ofs, sloejit::aarch64::zv_d);
    } else {
      ib.make_st1w_zpri(val_r, pred_full(ib, regmap, 32), base_r, ofs,
                        sloejit::aarch64::zv_d);
    }
    break;
  }
  case 16: {
    ASSERT(mask == 0b01010101 || mask == 0b11111111);
    if (mask == 0b11111111) {
      auto pred = use_pred ? pred_r : pred_full(ib, regmap, 16);
      ib.make_st1w_zpri(val_r, pred, base_r, ofs, sloejit::aarch64::zv_s);
    } else {
      ib.make_st1h_zpri(val_r, pred_full(ib, regmap, 16), base_r, ofs,
                        sloejit::aarch64::zv_s);
    }
    break;
  }
  default:
    ASSERT(false);
  }
}

static inline void emit_store_rri(sloejit::aarch64::instr_builder &ib,
                                  std::vector<rodata_info> &data_ofs,
                                  std::vector<uint8_t> &data_bytes,
                                  regmap_t &regmap, sloejit::reg base_r,
                                  int ofs, ir_value v, ir_value a) {
  ASSERT(a->type->nelems.is_contig());
  int total_width = a->type->nelems.count_contig() * a->type->elem_width;
  if (total_width == 8) {
    auto src_r = b_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_b_str_rri(src_r, base_r, ofs);
  } else if (total_width == 16) {
    auto src_r = h_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_h_str_rri(src_r, base_r, ofs);
  } else if (total_width == 32) {
    auto src_r = s_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_s_str_rri(src_r, base_r, ofs);
  } else if (total_width == 64) {
    auto src_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_d_str_rri(src_r, base_r, ofs);
  } else if (total_width == 128) {
    auto src_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_q_str_rri(src_r, base_r, ofs);
  } else {
    ASSERT(false);
  }
}

static inline void emit_store_rrr(sloejit::aarch64::instr_builder &ib,
                                  std::vector<rodata_info> &data_ofs,
                                  std::vector<uint8_t> &data_bytes,
                                  regmap_t &regmap, sloejit::reg base_r,
                                  sloejit::reg ofs_r, ir_value v, ir_value a) {
  ASSERT(a->type->nelems.is_contig());
  int total_width = a->type->nelems.count_contig() * a->type->elem_width;
  if (total_width == 8) {
    auto src_r = b_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_b_str_rrr(src_r, base_r, ofs_r);
  } else if (total_width == 16) {
    auto src_r = h_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_h_str_rrr(src_r, base_r, ofs_r);
  } else if (total_width == 32) {
    auto src_r = s_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_s_str_rrr(src_r, base_r, ofs_r);
  } else if (total_width == 64) {
    auto src_r = d_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_d_str_rrr(src_r, base_r, ofs_r);
  } else if (total_width == 128) {
    auto src_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    ib.make_q_str_rrr(src_r, base_r, ofs_r);
  } else {
    ASSERT(false);
  }
}

static inline void emit_whilelt(sloejit::aarch64::instr_builder &ib,
                                regmap_t &regmap, ir_value v) {
  ASSERT(v->op == IVO_WHILELT);
  ASSERT(v->type->kind == IVK_PREDICATE);
  ASSERT(v->deps.size() == 2);
  auto dst_r = p_(ib, regmap, v);
  auto zv = get_zv(v->type->elem_width);
  const auto a_val = v->deps[0];
  const auto b_val = v->deps[1];
  if (a_val->op == IVO_CONST_INT && b_val->op == IVO_CONST_INT) {
    switch ((int)b_val->literals[0] - (int)a_val->literals[0]) {
    case 1:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl1, zv);
    case 2:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl2, zv);
    case 3:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl3, zv);
    case 4:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl4, zv);
    case 5:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl5, zv);
    case 6:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl6, zv);
    case 7:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl7, zv);
    case 8:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl8, zv);
    case 16:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl16, zv);
    case 32:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl32, zv);
    case 64:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl64, zv);
    case 128:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl128, zv);
    case 256:
      return ib.make_ptrue(dst_r, sloejit::aarch64::ptrue_pat_vl256, zv);
    }
  }

  auto a = x_(ib, regmap, v->deps[0]);
  auto b = x_(ib, regmap, v->deps[1]);
  ib.make_whilelt_prr(dst_r, a, b, zv);
}

template<bool IsSVE, bool IsSME>
static inline void emit_scatter(sloejit::aarch64::instr_builder &ib,
                                std::vector<rodata_info> &data_ofs,
                                std::vector<uint8_t> &data_bytes,
                                regmap_t &regmap, ir_value v) {
  ASSERT(v->op == IVO_SCATTER);
  ASSERT(v->deps.size() == 3);
  // b[c] = a
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto c = v->deps[2];
  ASSERT(is_float_or_fixed_kind(a->type->kind));
  ASSERT(b->type->nelems.is_one());
  ASSERT(!c->type->nelems.is_one());

  if constexpr (IsSVE) {
    auto base_r = x_(ib, regmap, b);
    auto val_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    // scatter, arguments are a vector of values, scalar base, vector of offsets
    ASSERT(c->type->nelems.is_sve());
    if constexpr (IsSME) {
      ASSERT(is_sve_reverse_index(c) &&
             "Cannot emit this access pattern for SME");
      auto pred_r = pred_full(ib, regmap, a->type->elem_width);
      auto rev_pred_r = reverse_contiguous_pred(ib, pred_r, a->type);
      auto store_base_r = reverse_contiguous_base(ib, base_r, pred_r, a->type);
      auto rev_val_r =
          ib.make_rev_z(val_r, get_zv(effective_elem_bits(*a->type)));
      // Splice rev_val_r with itself - the second half of the vector will get
      // ignored anyway
      ib.make_splice_zpz(rev_val_r, rev_pred_r, rev_val_r,
                         get_zv(effective_elem_bits(*a->type)));
      emit_scatter_zpri(ib, data_ofs, data_bytes, regmap, rev_val_r,
                        store_base_r, 0, pred_r, true, a);
      return;
    }
    auto ofs_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, c);
    emit_scatter_zprz(ib, data_ofs, data_bytes, regmap, val_r, base_r, ofs_r,
                      p_true, false, a);
    return;
  }
  ASSERT(false && "unimplemented IVO_SCATTER");
}

template<bool IsSVE>
static inline void emit_store(sloejit::aarch64::instr_builder &ib,
                              std::vector<rodata_info> &data_ofs,
                              std::vector<uint8_t> &data_bytes,
                              regmap_t &regmap, ir_value v) {
  bool has_pred =
      v->deps.size() == 4 && v->deps[0]->type->kind == IVK_PREDICATE;
  ASSERT(v->op == IVO_STORE);
  ASSERT(v->deps.size() == 3 || has_pred);
  // b[c] = a
  auto a = has_pred ? v->deps[1] : v->deps[0];
  auto b = has_pred ? v->deps[2] : v->deps[1];
  auto c = has_pred ? v->deps[3] : v->deps[2];
  ASSERT(is_float_or_fixed_kind(a->type->kind));
  ASSERT(b->type->nelems.is_one());
  ASSERT(c->type->nelems.is_one());

  if constexpr (IsSVE) {
    auto base_r = x_(ib, regmap, b);
    auto val_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, a);
    auto pred_r = p_true;
    bool use_pred = false;
    if (has_pred) {
      auto pred = v->deps[0];
      pred_r = p_(ib, regmap, pred);
      use_pred = true;
    }

    if (is_constant_int_zero(c)) {
      emit_scatter_zpri(ib, data_ofs, data_bytes, regmap, val_r, base_r, 0,
                        pred_r, use_pred, a);
      return;
    }

    auto ofs_r = x_(ib, regmap, c);
    emit_scatter_zprr(ib, data_ofs, data_bytes, regmap, val_r, base_r, ofs_r,
                      pred_r, use_pred, a);
  } else {
    auto base_r = resolve_base_reg(ib, data_ofs, data_bytes, regmap, b);
    if (a->type->nelems == b->type->inner_type->nelems) {
      // try to replace a sequence such as:
      // movz x0, #n;  str src, [x1, x0, lsl #sh]
      // with:
      // str src, [x1, #(n << sh)]
      // Check whether c is a constant integer value
      if (c->op == IVO_CONST_INT) {
        ASSERT(c->literals.size() == 1);
        auto imm = c->literals[0];
        ASSERT(a->type->nelems.is_contig());
        int total_width = a->type->nelems.count_contig() * a->type->elem_width;
        auto ofs = (int)(imm * total_width / 8);
        if (ofs >= 0) {
          emit_store_rri(ib, data_ofs, data_bytes, regmap, base_r, ofs, v, a);
          // Remove the current operation as a use of the movz
          c->erase_use(v);
          return;
        }
      }

      auto ofs_r = x_(ib, regmap, c);
      emit_store_rrr(ib, data_ofs, data_bytes, regmap, base_r, ofs_r, v, a);
      return;
      // TODO: we should also have the same st1 optimization we used to have
      // here
      //       to avoid pointless concats
    }
    // wider than normal store, if e.g. loop unrolling has happened
    // note there is nothing to return here, since stores obviously do not
    // produce a value.
    ASSERT(c->op == IVO_CONST_INT && (int)c->literals[0] == 0);
    emit_store_rri(ib, data_ofs, data_bytes, regmap, base_r, 0, v, a);
  }
}

template<bool IsSVE>
static inline void emit_fconj(sloejit::aarch64::instr_builder &ib,
                              std::vector<rodata_info> &data_ofs,
                              std::vector<uint8_t> &data_bytes,
                              regmap_t &regmap, ir_value v) {
  ASSERT(v->deps.size() == 1);
  auto a = v->deps[0];
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);

  if constexpr (IsSVE) {
    auto zv = get_zv(a->type->elem_width);
    ib.make_orr_zzz(dst_r, ra, ra);
    ib.make_fneg_zpz(dst_r, pred_imag(ib, regmap, a->type->elem_width), ra, zv);
  } else {
    // emulate conjugation with a double-width fneg.
    ASSERT(a->type->elem_width <= 32);
    ASSERT(a->type->nelems.is_even());
    ASSERT(a->type->nelems.is_contig());
    int total_width = a->type->nelems.count_contig() * a->type->elem_width;
    auto qv = get_qv(total_width, a->type->elem_width * 2);
    ib.make_fneg_qq(dst_r, ra, qv);
  }
}

template<bool IsSVE>
static inline void emit_sqconj(sloejit::aarch64::instr_builder &ib,
                               std::vector<rodata_info> &data_ofs,
                               std::vector<uint8_t> &data_bytes,
                               regmap_t &regmap, ir_value v) {
  ASSERT(false && "SQCONJ not yet implemented");
}

template<bool IsSVE>
static inline void emit_fneg(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  // TODO: can use smaller NEON/scalar versions if !a->type->nelems.sve.
  ASSERT(v->deps.size() == 1);
  auto a = v->deps[0];
  auto v_variant = get_qzv<IsSVE>(a->type.get());
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  if constexpr (IsSVE) {
    // TODO: this can be movprfx + fneg
    z_zero(dst_r, ib);
    ib.make_fneg_zpz(dst_r, p_true, ra, v_variant);
  } else {
    ib.make_fneg_qq(dst_r, ra, v_variant);
  }
}

template<bool IsSVE>
static inline void emit_sqneg(sloejit::aarch64::instr_builder &ib,
                              std::vector<rodata_info> &data_ofs,
                              std::vector<uint8_t> &data_bytes,
                              regmap_t &regmap, ir_value v) {
  if constexpr (IsSVE) {
    ASSERT(false && "SQNEG not yet implemented for SVE");
  }
  // TODO: can use smaller NEON versions if !a->type->nelems.sve.
  ASSERT(v->deps.size() == 1);
  auto a = v->deps[0];
  auto v_variant = get_qzv<false>(a->type.get());
  auto ra = get_r<false>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto dst_r = get_r<false>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  ib.make_sqneg_qq(dst_r, ra, v_variant);
}

template<bool IsSVE>
static inline void emit_srshr(sloejit::aarch64::instr_builder &ib,
                              std::vector<rodata_info> &data_ofs,
                              std::vector<uint8_t> &data_bytes,
                              regmap_t &regmap, ir_value v) {
  if constexpr (IsSVE) {
    ASSERT(false && "srshr not implemented for SVE");
  }

  ASSERT(v->deps.size() == 1);
  auto a = v->deps[0];

  ASSERT(v->type->nelems == a->type->nelems);
  ASSERT(v->type->nelems.is_contig());

  auto v_variant = get_qzv<false>(a->type.get());
  auto ra = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, a);
  auto dst_r = get_r<IsSVE>(v->scope, ib, data_ofs, data_bytes, regmap, v);
  ASSERT(v->literals.size() == 1);
  auto shift = v->literals[0];
  ib.make_srshr_qqi(dst_r, ra, shift, v_variant);
}

template<bool IsSVE>
static inline void emit_cast(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  if constexpr (IsSVE) {
    ASSERT(false && "IVO_CAST not implemented for SVE");
  } else {
    ASSERT(v->deps.size() == 1);
    auto a = v->deps[0];
    ASSERT(v->type->nelems == a->type->nelems);
    auto from = a->type->elem_width;
    auto to = v->type->elem_width;
    if (!v->type->nelems.is_one()) {
      auto ra = q_(v->scope, ib, data_ofs, data_bytes, regmap, a);
      auto dst_r = q_(v->scope, ib, data_ofs, data_bytes, regmap, v);
      if (from == 16 && to == 32) {
        ib.make_fcvtl_qq(dst_r, ra, sloejit::aarch64::qv_4s,
                         sloejit::aarch64::qv_4h);
        return;
      } else if (from == 32 && to == 16) {
        ib.make_fcvtn_qq(dst_r, ra, sloejit::aarch64::qv_4h,
                         sloejit::aarch64::qv_4s);
        return;
      } else {
        ASSERT(false && "unsupported >1 elem cast");
      }
    }
    if (from == 16 && to == 32) {
      auto ra = h_(v->scope, ib, data_ofs, data_bytes, regmap, a);
      auto dst_r = s_(v->scope, ib, data_ofs, data_bytes, regmap, v);
      ib.make_fcvt_sh(dst_r, ra);
      return;
    } else if (from == 32 && to == 16) {
      auto ra = s_(v->scope, ib, data_ofs, data_bytes, regmap, a);
      auto dst_r = h_(v->scope, ib, data_ofs, data_bytes, regmap, v);
      ib.make_fcvt_hs(dst_r, ra);
      return;
    } else {
      ASSERT(false && "unsupported 1 elem cast");
    }
  }
}

static inline void emit_iadd(sloejit::aarch64::instr_builder &ib,
                             regmap_t &regmap, ir_value v, sloejit::reg dst_r,
                             sloejit::reg ra, ir_value b) {
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());
  // try to replace a sequence such as:
  // movz x0, #n;  add x2, x1, x0
  // with:
  // add x2, x1, #n
  if (b->op == IVO_CONST_INT) {
    ASSERT(b->literals.size() == 1);
    int imm = b->literals[0];
    // TODO: if imm==0 we could emit a mov instead (or just remove this
    // entirely!)
    if (imm < 0) {
      ib.make_x_sub_rri(dst_r, ra, -imm);
    } else {
      ib.make_x_add_rri(dst_r, ra, imm);
    }
    if (v) {
      b->erase_use(v);
    }
  } else {
    auto rb = x_(ib, regmap, b);
    ib.make_x_add_rrr(dst_r, ra, rb, sloejit::aarch64::lsl_0);
  }
}

static inline void emit_iadd(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());

  if (a->op == IVO_CONST_INT) {
    std::swap(a, b);
  }
  auto dst_r = x_(ib, regmap, v);
  auto ra = x_(ib, regmap, a);
  if (a->id == b->id) {
    // a + a could also just be a << 1
    ib.make_x_add_rrr(dst_r, ra, ra, sloejit::aarch64::lsl_0);
  } else {
    emit_iadd(ib, regmap, v, dst_r, ra, b);
  }
}

static inline void emit_isub(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());

  auto dst_r = x_(ib, regmap, v);
  auto ra = x_(ib, regmap, a);

  // try to replace a sequence such as:
  // movz x0, #n;  sub x2, x1, x0
  // with:
  // sub x2, x1, #n
  if (b->op == IVO_CONST_INT) {
    ASSERT(b->literals.size() == 1);
    auto imm = b->literals[0];
    // TODO: if imm==0 we could emit a mov instead (or just remove this
    // entirely!)
    ib.make_x_sub_rri(dst_r, ra, (int)imm);
    if (v) {
      b->erase_use(v);
    }
  } else {
    auto rb = x_(ib, regmap, b);
    ib.make_x_sub_rrr(dst_r, ra, rb, sloejit::aarch64::lsl_0);
  }
}

static inline void emit_idiv(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());

  auto dst_r = x_(ib, regmap, v);
  auto ra = x_(ib, regmap, a);
  // Try to replace a sequence such as:
  // movz x0, #(1 << n); sdiv x2, x1, x0
  // with:
  // asr x2, x1, #n
  auto maybe_n = get_pow2_lsl_int(b);
  if (maybe_n) {
    // We've got the n, and now we want to emit an asr instruction.
    ib.make_asr_rri(dst_r, ra, *maybe_n);
    return;
  }
  auto rb = x_(ib, regmap, b);
  ib.make_x_sdiv_rrr(dst_r, ra, rb);
}

static inline void emit_imod(sloejit::aarch64::instr_builder &ib,
                             std::vector<rodata_info> &data_ofs,
                             std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                             ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());

  auto dst_r = x_(ib, regmap, v);
  auto ra = x_(ib, regmap, a);
  // Try to replace a sequence such as:
  // movz x0, #(1 << n); sdiv x2, x1, x0; mul x2, x0, x2
  // with:
  // movz x0, #((1 << n) - 1); and x2, x1, x0
  auto maybe_n = get_pow2_lsl_int(b);
  if (maybe_n) {
    // We've got the n, and now we want to emit an and instruction.
    ib.make_x_and_rrr(dst_r, ra, ib.make_x_movz_i((1 << *maybe_n) - 1));
    return;
  }
  auto rb = x_(ib, regmap, b);
  auto rdiv = ib.make_x_sdiv_rr(ra, rb);
  auto rmul = ib.make_x_mul_rr(rb, rdiv);
  ib.make_x_sub_rrr(dst_r, ra, rmul, sloejit::aarch64::lsl_0);
}

template<bool IsSVE>
static inline void emit_index(sloejit::aarch64::instr_builder &ib,
                              std::vector<rodata_info> &data_ofs,
                              std::vector<uint8_t> &data_bytes,
                              regmap_t &regmap, ir_value v) {
  // IVO_INDEX for NEON is a no-op.
  if constexpr (IsSVE) {
    ASSERT(v->deps.size() == 2);
    auto a = v->deps[0];
    auto b = v->deps[1];
    auto dst_r = z_(v->scope, ib, data_ofs, data_bytes, regmap, v);
    std::optional<int> maybe_a_imm;
    std::optional<int> maybe_b_imm;
    if (a->op == IVO_CONST_INT) {
      const int a_val = a->literals[0];
      if (a_val >= -16 && a_val <= 15) {
        maybe_a_imm = a_val;
      }
    }
    if (b->op == IVO_CONST_INT) {
      const int b_val = b->literals[0];
      if (b_val >= -16 && b_val <= 15) {
        maybe_b_imm = b_val;
      }
    }
    // use a 32-bit increment for half-precision kernels: index z0.s, w6, w7
    // and   64-bit otherwise                           : index z0.d, x6, x7
    auto zv = v->type->elem_width == 32 ? sloejit::aarch64::zv_s
                                        : sloejit::aarch64::zv_d;

    if (maybe_a_imm && maybe_b_imm) {
      ib.make_index_zii(dst_r, *maybe_a_imm, *maybe_b_imm, zv);
    } else if (maybe_a_imm) {
      ib.make_index_zir(dst_r, *maybe_a_imm, x_(ib, regmap, b), zv);
    } else if (maybe_b_imm) {
      ib.make_index_zri(dst_r, x_(ib, regmap, a), *maybe_b_imm, zv);
    } else {
      ib.make_index_zrr(dst_r, x_(ib, regmap, a), x_(ib, regmap, b), zv);
    }
  }
}

static inline void emit_eq_sel(sloejit::aarch64::instr_builder &ib,
                               std::vector<rodata_info> &data_ofs,
                               std::vector<uint8_t> &data_bytes,
                               regmap_t &regmap, ir_value v) {
  ASSERT(v->deps.size() == 4);
  auto a = v->deps[0];
  auto b = v->deps[1];
  auto c = v->deps[2];
  auto d = v->deps[3];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());

  auto dst_r = x_(ib, regmap, v);
  auto ra = x_(ib, regmap, a);
  auto rb = x_(ib, regmap, b);
  auto rc = x_(ib, regmap, c);
  auto rd = x_(ib, regmap, d);
  ib.make_x_cmp_rr(ra, rb);
  ib.make_x_csel_eq_rrr(dst_r, rc, rd);
}

static inline void emit_min(sloejit::aarch64::instr_builder &ib,
                            std::vector<rodata_info> &data_ofs,
                            std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                            ir_value v) {
  ASSERT(v->deps.size() == 2);
  auto a = v->deps[0];
  auto b = v->deps[1];
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());

  auto ra = x_(ib, regmap, a);
  auto rb = x_(ib, regmap, b);
  auto dst_r = x_(ib, regmap, v);

  if (b->op == IVO_CONST_INT) {
    const int b_val = b->literals[0];
    const bool b_can_be_imm =
        ((b_val & 0xfff) == b_val || (b_val & 0xfff000) == b_val);
    if (b_can_be_imm) {
      ib.make_x_cmp_ri(ra, b_val);
      ib.make_x_csel_le_rrr(dst_r, ra, rb);
      return;
    }
  }
  if (a->op == IVO_CONST_INT) {
    const int a_val = a->literals[0];
    const bool a_can_be_imm =
        ((a_val & 0xfff) == a_val || (a_val & 0xfff000) == a_val);
    if (a_can_be_imm) {
      ib.make_x_cmp_ri(rb, a_val);
      ib.make_x_csel_ge_rrr(dst_r, ra, rb);
      return;
    }
  }
  ib.make_x_cmp_rr(ra, rb);
  ib.make_x_csel_le_rrr(dst_r, ra, rb);
}

static inline void emit_param(sloejit::stack_frame_info *frame_info,
                              sloejit::aarch64::instr_builder &ib,
                              regmap_t &regmap, ir_value v) {
  if (v->str == "j") {
    // TODO: this mov shouldn't need to exist?
    auto dst_r = x_(ib, regmap, v);
    ib.make_x_mov_rr(dst_r, regmap.at(j_var).reg);
    return;
  }
  ASSERT(v->literals.size() == 1);
  // parameter is either a pointer or an integer, in reg x{n}.
  int param_num = (int)v->literals[0];
  if (param_num < 8) {
    // TODO: this mov shouldn't need to exist?
    auto dst_r = x_(ib, regmap, v);
    ib.make_x_mov_rr(dst_r, regmap.at(-param_num - 1).reg);
    return;
  }
  param_num -= 8;
  // remember to 16-byte align the stack, else we get a SIGBUS!
  int this_arg_stack_size_bytes = 8 * ((param_num + 1) + ((param_num + 1) % 2));
  frame_info->parent_call_arg_stack_size = std::max(
      frame_info->parent_call_arg_stack_size, this_arg_stack_size_bytes);
  ASSERT(frame_info->parent_call_arg_stack_size % 16 == 0);
  auto dst_r = x_(ib, regmap, v);
  ib.make_x_ldr_rri(dst_r, sloejit::aarch64::x29, param_num * 8);
}

static inline void emit_ctrl_cmp(sloejit::aarch64::instr_builder &ib,
                                 regmap_t &regmap, sloejit::reg ra,
                                 ir_value b) {
  // note that this function cannot be used other than for control flow, since
  // it does not erase b's uses in the case the immediate pattern is used!
  // (see emit_iadd for an example of how it could be done if needed).
  if (b->op == IVO_CONST_INT) {
    ASSERT(b->literals.size() == 1);
    int imm = b->literals[0];
    ib.make_x_cmp_ri(ra, imm);
  } else {
    ib.make_x_cmp_rr(ra, x_(ib, regmap, b));
  }
}

} // end namespace plfft::wfta
