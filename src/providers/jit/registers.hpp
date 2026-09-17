/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irprinter.hpp"
#include "irprinter_common.hpp"
#include "irvalue.hpp"
#include "irvalue_scope_iterator.hpp"
#include "plfft_assert.hpp"

#include "sloejit/aarch64/aarch64.hpp"

namespace plfft::wfta {

template<sloejit::aarch64::preg_classes reg_class, bool IsSVE>
static sloejit::reg
reg_common(ir_value_scope *scope, sloejit::aarch64::instr_builder &ib,
           std::vector<rodata_info> &data_ofs, std::vector<uint8_t> &data_bytes,
           regmap_t &regmap, ir_value v);

/** Get an x register if it is not a constant,
 *  else emit a literal mov to create it as-needed.
 *
 * @param[in,out] ib The instruction builder for the current block.
 * @param[in] regmap The current register mapping state
 *                   (from ir_value id to sloejit register).
 * @param[in] v      The integer value being emitted.
 */
static sloejit::reg x_(sloejit::aarch64::instr_builder &ib, regmap_t &regmap,
                       ir_value v) {
  if (v->op == IVO_CONST_INT) {
    ASSERT(v->literals.size() == 1);
    int val = (int)v->literals[0];
    ASSERT(val >= -65536 && val < 65536);
    if (val < 0) {
      // use movn (mov bitwise-not'ed immediate) to get negative numbers.
      return ib.make_x_movn_i(~val);
    } else {
      return ib.make_x_movz_i(val);
    }
  }
  auto it = regmap.find(v->id);
  if (it != regmap.end()) {
    return it->second.reg;
  }
  auto r =
      ib.b->fresh_vreg(sloejit::aarch64::x_space, sloejit::aarch64::x_regs);
  ASSERT(regmap.emplace(v->id, r).second);
  return regmap.at(v->id).reg;
}

static sloejit::reg b_(ir_value_scope *scope,
                       sloejit::aarch64::instr_builder &ib,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                       ir_value v) {
  return reg_common<sloejit::aarch64::b_regs, false>(scope, ib, data_ofs,
                                                     data_bytes, regmap, v);
}

static sloejit::reg h_(ir_value_scope *scope,
                       sloejit::aarch64::instr_builder &ib,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                       ir_value v) {
  return reg_common<sloejit::aarch64::h_regs, false>(scope, ib, data_ofs,
                                                     data_bytes, regmap, v);
}

static sloejit::reg s_(ir_value_scope *scope,
                       sloejit::aarch64::instr_builder &ib,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                       ir_value v) {
  return reg_common<sloejit::aarch64::s_regs, false>(scope, ib, data_ofs,
                                                     data_bytes, regmap, v);
}

static sloejit::reg d_(ir_value_scope *scope,
                       sloejit::aarch64::instr_builder &ib,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                       ir_value v) {
  return reg_common<sloejit::aarch64::d_regs, false>(scope, ib, data_ofs,
                                                     data_bytes, regmap, v);
}

static sloejit::reg q_(ir_value_scope *scope,
                       sloejit::aarch64::instr_builder &ib,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                       ir_value v) {
  return reg_common<sloejit::aarch64::q_regs, false>(scope, ib, data_ofs,
                                                     data_bytes, regmap, v);
}

static sloejit::reg z_(ir_value_scope *scope,
                       sloejit::aarch64::instr_builder &ib,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                       ir_value v) {
  return reg_common<sloejit::aarch64::z_regs, true>(scope, ib, data_ofs,
                                                    data_bytes, regmap, v);
}

static inline sloejit::aarch64::q_type_variant get_qv(int total_width,
                                                      int elem_width) {
  // TODO: for most uses of this function, it would be better to sometimes
  //       emit a scalar operation (e.g. fadd d0, d0, d1)
  ASSERT(total_width % elem_width == 0);
  auto qv = total_width == 128 && elem_width == 8    ? sloejit::aarch64::qv_16b
            : total_width != 128 && elem_width == 8  ? sloejit::aarch64::qv_8b
            : total_width == 128 && elem_width == 16 ? sloejit::aarch64::qv_8h
            : total_width != 128 && elem_width == 16 ? sloejit::aarch64::qv_4h
            : total_width == 128 && elem_width == 32 ? sloejit::aarch64::qv_4s
            : total_width != 128 && elem_width == 32 ? sloejit::aarch64::qv_2s
            : total_width == 128 && elem_width == 64 ? sloejit::aarch64::qv_2d
            : total_width != 128 && elem_width == 64
                ? sloejit::aarch64::qv_2d // intentional (there's no .1d suffix)
                : (sloejit::aarch64::q_type_variant)0;
  ASSERT(qv);
  return qv;
}

static inline sloejit::aarch64::q_type_variant
get_qv_bytes(int elem_width, ir_value_num_elems nelems) {
  ASSERT(!nelems.is_sve());
  ASSERT(nelems.is_contig());
  int total_width = nelems.count_contig() * elem_width;
  return total_width > 64 ? sloejit::aarch64::qv_16b : sloejit::aarch64::qv_8b;
}

static inline sloejit::aarch64::z_type_variant get_zv(int elem_width) {
  auto zv = elem_width == 8     ? sloejit::aarch64::zv_b
            : elem_width == 16  ? sloejit::aarch64::zv_h
            : elem_width == 32  ? sloejit::aarch64::zv_s
            : elem_width == 64  ? sloejit::aarch64::zv_d
            : elem_width == 128 ? sloejit::aarch64::zv_q
                                : (sloejit::aarch64::z_type_variant)0;
  ASSERT(zv);
  return zv;
}

template<bool IsSVE>
static inline sloejit::reg (*get_r)(ir_value_scope *,
                                    sloejit::aarch64::instr_builder &,
                                    std::vector<rodata_info> &,
                                    std::vector<uint8_t> &, regmap_t &,
                                    ir_value);
template<>
inline constexpr auto get_r<true> = &z_;
template<>
inline constexpr auto get_r<false> = &q_;

template<bool IsSVE>
std::conditional_t<IsSVE, sloejit::aarch64::z_type_variant,
                   sloejit::aarch64::q_type_variant>
get_qzv(ir_value_type *t) {
  if constexpr (IsSVE) {
    return get_zv(t->elem_width);
  } else {
    ASSERT(t->nelems.is_contig());
    int total_width = t->nelems.count_contig() * t->elem_width;
    return get_qv(total_width, t->elem_width);
  }
}

// this are just constants to index into the regmap that won't conflict with
// any named variables (which all have positive ids) or input parameters
// (which are numbered from -1 downwards).
static constexpr int j_var = -999;
static constexpr int j_lim_var = -998;

// data: [re, im,  re,  im,  X,   X,   X,   X  ],
// inc = 4 complex numbers
// lim = 2 (hence the Xs)
// clang-format off
// pred_full: [1,   1,   1,   1,   0,   0,   0,   0  ]
// pred_real: [1,   0,   1,   0,   0,   0,   0,   0  ]
// pred_imag: [0,   1,   0,   1,   0,   0,   0,   0  ]
// pred_half: [1,   1,   0,   0,   0,   0,   0,   0  ]
// half:   j: [j,                  j+1,              ] <-- one 64b idx per 16b elems
// float:  j: [j,        j+1,      j+2,      j+3,    ] <-- one 64b idx per 2 32b elems
// double: j: [j,   j,   j+1, j+1, j+2, j+2, j+3, j+3] <-- one 64b idx per 1 64b elem
// clang-format on
static constexpr sloejit::reg p_true = sloejit::aarch64::p0;
static constexpr int pred_full_var = -997;
static constexpr int pred_real_var = -996;
static constexpr int pred_imag_var = -995;
static constexpr int pred_half_var = -994;

static sloejit::reg p_(sloejit::aarch64::instr_builder &ib, regmap_t &regmap,
                       ir_value v) {
  ASSERT(v->type->kind == IVK_PREDICATE);
  if (v->op == IVO_PTRUE) {
    auto zv = get_zv(v->type->elem_width);
    ASSERT(zv == sloejit::aarch64::zv_b);
    return p_true;
  }
  auto it = regmap.find(v->id);
  if (it != regmap.end()) {
    return it->second.reg;
  }
  auto r =
      ib.b->fresh_vreg(sloejit::aarch64::p_space, sloejit::aarch64::p_regs);
  ASSERT(regmap.emplace(v->id, r).second);
  return r;
}

static inline sloejit::reg pred_full(sloejit::aarch64::instr_builder &ib,
                                     regmap_t &regmap, int elem_width) {
  // use the predicate if it exists, else build it for the given element width
  auto it = regmap.find(pred_full_var);
  if (it != regmap.end() &&
      try_instr_ensure_many_before(ib, it->second.origin, 3)) {
    return it->second.reg;
  }
  sloejit::reg r;
  switch (elem_width) {
  case 16:
    r = ib.make_whilelt_rr(ib.make_lsl_ri(regmap.at(j_var).reg, 1),
                           ib.make_lsl_ri(regmap.at(j_lim_var).reg, 1),
                           sloejit::aarch64::zv_h);
    break;
  case 32:
    r = ib.make_whilelt_rr(ib.make_lsl_ri(regmap.at(j_var).reg, 1),
                           ib.make_lsl_ri(regmap.at(j_lim_var).reg, 1),
                           sloejit::aarch64::zv_s);
    break;
  case 64:
    r = ib.make_whilelt_rr(ib.make_lsl_ri(regmap.at(j_var).reg, 1),
                           ib.make_lsl_ri(regmap.at(j_lim_var).reg, 1),
                           sloejit::aarch64::zv_d);
    break;
  default:
    ASSERT(false);
  }
  regmap[pred_full_var].reg = r;
  regmap[pred_full_var].origin = ib.get_last_inserted_instr();
  return r;
}

static inline sloejit::reg pred_real(sloejit::aarch64::instr_builder &ib,
                                     regmap_t &regmap, int elem_width) {
  // use the predicate if it exists, else derive it from pred_full
  auto it_real = regmap.find(pred_real_var);
  auto it_full = regmap.find(pred_full_var);
  if (it_real != regmap.end() &&
      try_instr_ensure_many_before(ib, it_full->second.origin, 3) &&
      try_instr_ensure_many_before(ib, it_real->second.origin, 2)) {
    return it_real->second.reg;
  }
  regmap[pred_real_var].reg = ib.make_trn1_pp(
      pred_full(ib, regmap, elem_width), ib.make_pfalse(), get_zv(elem_width));
  regmap[pred_real_var].origin = ib.get_last_inserted_instr();
  return regmap[pred_real_var].reg;
}

static inline sloejit::reg pred_imag(sloejit::aarch64::instr_builder &ib,
                                     regmap_t &regmap, int elem_width) {
  // use the predicate if it exists, else derive it from pred_full
  auto it_imag = regmap.find(pred_imag_var);
  auto it_full = regmap.find(pred_full_var);
  if (it_imag != regmap.end() &&
      try_instr_ensure_many_before(ib, it_full->second.origin, 3) &&
      try_instr_ensure_many_before(ib, it_imag->second.origin, 2)) {
    return it_imag->second.reg;
  }
  regmap[pred_imag_var].reg = ib.make_trn1_pp(
      ib.make_pfalse(), pred_full(ib, regmap, elem_width), get_zv(elem_width));
  regmap[pred_imag_var].origin = ib.get_last_inserted_instr();
  return regmap[pred_imag_var].reg;
}

static inline sloejit::reg pred_half(sloejit::aarch64::instr_builder &ib,
                                     regmap_t &regmap, int elem_width) {
  // use the predicate if it exists, else derive it from pred_full
  auto it_half = regmap.find(pred_half_var);
  auto it_full = regmap.find(pred_full_var);
  if (it_half != regmap.end() &&
      try_instr_ensure_many_before(ib, it_full->second.origin, 3) &&
      try_instr_ensure_many_before(ib, it_half->second.origin, 2)) {
    return it_half->second.reg;
  }
  regmap[pred_half_var].reg = ib.make_uzp1_pp(
      pred_full(ib, regmap, elem_width), ib.make_pfalse(), get_zv(elem_width));
  regmap[pred_half_var].origin = ib.get_last_inserted_instr();
  return regmap[pred_half_var].reg;
}

template<sloejit::aarch64::preg_classes reg_class, bool IsSVE>
static sloejit::reg
reg_common(ir_value_scope *scope, sloejit::aarch64::instr_builder &ib,
           std::vector<rodata_info> &data_ofs, std::vector<uint8_t> &data_bytes,
           regmap_t &regmap, ir_value v) {
  if (v->op == IVO_REINTERPRET) {
    ASSERT(v->deps.size() == 1);
    return sloejit::aarch64::reg_reinterpret_with_class(
        reg_common<reg_class, IsSVE>(scope, ib, data_ofs, data_bytes, regmap,
                                     v->deps[0]),
        reg_class);
  }
  // Get a register if it is not a constant, else emit a load from the literal
  // pool to create it.
  if (is_constant_value(v)) {
    auto pred = IsSVE ? &p_true : nullptr;
    return sloejit::aarch64::reg_reinterpret_with_class(
        get_literal_pool_value<IsSVE>(ib, regmap, data_ofs, data_bytes, v,
                                      pred),
        reg_class);
  }
  if constexpr (IsSVE) {
    if (v->op == IVO_GET_GROUP_OP) {
      ASSERT(v->deps.size() == 1);
      ASSERT(v->deps[0]->op == IVO_STRUCTURE_LOAD_GROUP);
      ASSERT(v->literals.size() == 1);
      auto it = regmap.find(v->id);
      if (it == regmap.end()) {
        regmap[v->id].reg = ib.b->fresh_vreg(sloejit::aarch64::v_space,
                                             sloejit::aarch64::z_regs);
      }
      return sloejit::aarch64::reg_reinterpret_with_class(regmap.at(v->id).reg,
                                                          reg_class);
    }
  }
  auto it = regmap.find(v->id);
  if (it != regmap.end()) {
    return sloejit::aarch64::reg_reinterpret_with_class(it->second.reg,
                                                        reg_class);
  }
  auto r = ib.b->fresh_vreg(sloejit::aarch64::v_space, reg_class);
  ASSERT(regmap.emplace(v->id, r).second);
  return r;
}

} // end namespace plfft::wfta
