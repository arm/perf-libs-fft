/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irvalue.hpp"
#include "irvalue_scope_iterator.hpp"
#include "plfft_assert.hpp"
#include "plfft_convert.hpp"
#include "plfft_util.hpp"
#include "polyval.hpp"
#include "sloejit/aarch64/aarch64.hpp"

#include <cstring>
#include <map>
#include <optional>

namespace plfft::wfta {

/// this is just a constant to index into the regmap that won't conflict with
/// any named variables (which all have positive ids) or input parameters
/// (which are numbered from -1 downwards).
static constexpr int rodata_var = -500;

static inline bool is_constant_int_zero(ir_value v) {
  return v->op == IVO_CONST_INT && v->literals[0] == 0;
}

/** Enum describing whether something is a constant float or fixed value.
 *
 *  Constants may simply be IVO_CONST_{FLOAT,FIXED} values or concatenations
 *  of existing constant values. In principle there are other obvious things
 *  that are constants (shuffles, fadds, etc) but these are not emitted in
 *  practice and so are ignored.
 */
enum const_res {
  CONST_FALSE, ///< The value is not a float or fixed constant.
  CONST_HERE,  ///< The value is constant, but should be emitted here to
               ///< avoid inserting loads into a loop body.
  CONST_TRUE   ///< The value is a float or fixed constant.
};

/// Figure out the const_res classification of the specified value.
static inline const_res is_constant_value(ir_value v) {
  if (v->op != IVO_CONST_FLOAT && v->op != IVO_CONST_FIXED &&
      v->op != IVO_CONCAT) {
    return CONST_FALSE;
  }
  for (auto d : v->deps) {
    if (!is_constant_value(d)) {
      return CONST_FALSE;
    }
  }
  for (const auto &use_info : v->uses) {
    if (use_info.v->scope != v->scope) {
      return CONST_HERE;
    }
  }
  return CONST_TRUE;
}

static inline bool is_constant_value_here(ir_value v) {
  return is_constant_value(v) == CONST_HERE;
}

static inline bool is_constant_value_true(ir_value v) {
  return is_constant_value(v) == CONST_TRUE;
}

struct polyval_sized {
  int width;
  polyval data;
};

/// Evaluate the specified constant float or fixed value into a polyval bytes
/// representation, returning a pair of the data and its width.
static inline polyval_sized get_constant_value(ir_value v) {
  if (v->op == IVO_CONST_FLOAT) {
    ASSERT(v->type->nelems.is_one());
    polyval ret;
    switch (v->type->elem_width) {
    case 16:
      ret.fp16[0] = (__fp16)v->literals[0];
      return {2, ret};
    case 32:
      ret.fp32[0] = (float)v->literals[0];
      return {4, ret};
    case 64:
      ret.fp64[0] = (double)v->literals[0];
      return {8, ret};
    default:
      ASSERT(false);
    }
  }
  if (v->op == IVO_CONST_FIXED) {
    ASSERT(v->type->nelems.is_one());
    polyval ret;
    switch (v->type->elem_width) {
    case 8:
      ret.s8[0] = convert_to<int8_t>{}((float)v->literals[0]);
      return {1, ret};
    case 16:
      ret.s16[0] = convert_to<int16_t>{}((float)v->literals[0]);
      return {2, ret};
    case 32:
      ret.s32[0] = convert_to<int32_t>{}((float)v->literals[0]);
      return {4, ret};
    default:
      ASSERT(false);
    }
  }
  if (v->op == IVO_CONCAT) {
    const auto &elems = v->deps;
    const auto n = elems.size();
    ASSERT(n == 1 || n == 2 || n == 4 || n == 8);

    auto [nbytes_elem, ret] = get_constant_value(elems[0]);
    auto nbytes_acc = nbytes_elem;
    ASSERT(nbytes_elem * n <= 16);
    for (size_t i = 1; i < n; i++) {
      ASSERT(elems[i]->type->nelems.is_one());
      auto [nbytes, val] = get_constant_value(elems[i]);
      ASSERT(nbytes == nbytes_elem);
      memcpy(&ret.bytes[nbytes_acc], &val.bytes[0], nbytes_elem);
      nbytes_acc += nbytes_elem;
    }

    return {nbytes_acc, ret};
  }
  ASSERT(false);
}

static inline std::optional<int> get_pow2_lsl_int(int x) {
  switch (x) {
  case 1:
    return 0;
  case 2:
    return 1;
  case 4:
    return 2;
  case 8:
    return 3;
  case 16:
    return 4;
  case 32:
    return 5;
  case 64:
    return 6;
  case 128:
    return 7;
  }
  return std::nullopt;
}

static inline sloejit::aarch64::shift_amount get_pow2_lsl(int x) {
  return (sloejit::aarch64::shift_amount)(sloejit::aarch64::lsl_0 +
                                          *get_pow2_lsl_int(x));
}

static inline std::optional<int> get_pow2_lsl_int(ir_value v) {
  if (v->op != IVO_CONST_INT) {
    return std::nullopt;
  }
  ASSERT(v->literals.size() == 1);
  return get_pow2_lsl_int((int)(v->literals[0]));
}

static inline std::optional<sloejit::aarch64::shift_amount>
get_pow2_plus1_lsl(ir_value v) {
  if (v->op != IVO_CONST_INT) {
    return std::nullopt;
  }
  ASSERT(v->literals.size() == 1);
  auto sh = get_pow2_lsl_int((int)v->literals[0] - 1);
  if (!sh) {
    return std::nullopt;
  }
  return (sloejit::aarch64::shift_amount)(sloejit::aarch64::lsl_0 + *sh);
}

static bool try_instr_ensure_before(sloejit::aarch64::instr_builder &ib,
                                    sloejit::instruction *origin) {
  if (!origin) {
    return false;
  }
  // if in a different block, ignore it.
  if (origin->parent != ib.b) {
    return false;
  }
  if (origin == ib.instr_pos) {
    // avoid moving the instr_builder insertion point with us.
    ib.instr_pos = origin->instr_next;
    ASSERT(!ib.instr_pos || origin->pos <= ib.instr_pos->pos);
  } else if (ib.instr_pos && ib.b->instr_occurs_before(ib.instr_pos, origin)) {
    // origin was after current builder position, reposition the origin earlier
    ib.b->adopt(ib.b->orphan(origin), ib.instr_pos);
    ASSERT(!ib.instr_pos || origin->pos <= ib.instr_pos->pos);
  }
  return true;
}

static bool try_instr_ensure_many_before(sloejit::aarch64::instr_builder &ib,
                                         sloejit::instruction *origin, int n) {
  ASSERT(n > 0);
  if (!origin) {
    return false;
  }
  if (n > 1) {
    if (!try_instr_ensure_many_before(ib, origin->instr_prev, n - 1)) {
      return false;
    }
  }
  return try_instr_ensure_before(ib, origin);
}

static inline sloejit::reg get_rodata_addr(sloejit::aarch64::instr_builder &ib,
                                           regmap_t &regmap) {
  auto it2 = regmap.find(rodata_var);
  if (it2 != regmap.end() &&
      try_instr_ensure_many_before(ib, it2->second.origin, 2)) {
    ASSERT(!ib.instr_pos || it2->second.origin->pos <= ib.instr_pos->pos);
    return it2->second.reg;
  }
  auto *rodata = &ib.b->parent->rodata;
  auto addr = ib.make_x_add_rb(ib.make_adrp_b(rodata), rodata);
  regmap[rodata_var].reg = addr;
  regmap[rodata_var].origin = ib.get_last_inserted_instr();
  return addr;
}

/** To avoid long chains of code constructing constants, we choose
 *  to put these in read-only memory and simply emit a load instead.
 *
 * @param[in,out] ib         The instruction builder for the current block.
 * @param[in,out] data_ofs   A vector keeping track of existing data in the
 * literal pool, to allow us to reuse existing load of the literals.
 * @param[in,out] data_bytes The actual bytes of the literal pool itself.
 * @param[in] v              The value being emitted as a literal-pool load.
 * @param[in] pred           For SVE loads, the predicate to use when loading.
 */
template<bool HaveSVE>
static sloejit::reg
get_literal_pool_value(sloejit::aarch64::instr_builder &ib, regmap_t &regmap,
                       std::vector<rodata_info> &data_ofs,
                       std::vector<uint8_t> &data_bytes, ir_value v,
                       const sloejit::reg *pred = nullptr) {

  // Check whether this const is the lower part of a const FP concat
  // If it is, then we can just reuse the same literal
  for (const auto &use_info : v->uses) {
    auto *u = use_info.v;
    if (u->op == IVO_CONCAT && is_constant_value(u) && u->deps[0] == v) {
      return get_literal_pool_value<HaveSVE>(ib, regmap, data_ofs, data_bytes,
                                             u, pred);
    }
  }

  auto val = get_constant_value(v);

  // possibly replicate the value across the rest of the constant, in case it
  // is already used in a widened context or in case we want to use it as such
  // later.
  for (int i = val.width; i < 16; ++i) {
    val.data.bytes[i] = val.data.bytes[i % val.width];
  }

  // reuse already-loaded literal, if possible
  auto it = std::find_if(data_ofs.begin(), data_ofs.end(), [&](auto &elem) {
    return elem.val == val.data && elem.nelems == v->type->nelems;
  });
  if (it != data_ofs.end()) {
    if (it->origin->parent == ib.b) {
      // even if we can reuse a register for the literal, there is no guarantee
      // that the register is not initialized _after_ the current instruction,
      // so shuffle the load (rodata setup + the origin instr itself) earlier if
      // needed.
      get_rodata_addr(ib, regmap);
      ASSERT(try_instr_ensure_many_before(ib, it->origin, it->origin_len));
    }
    return it->r;
  }

  unsigned ofs = 0;
  auto ofs_it = std::find_if(data_ofs.begin(), data_ofs.end(),
                             [&](auto &elem) { return elem.val == val.data; });
  if (ofs_it != data_ofs.end()) {
    ofs = ofs_it->ofs;
  } else {
    ofs = (unsigned)data_bytes.size();
    for (int i = 0; i < 16; ++i) {
      data_bytes.push_back(val.data.bytes[i]);
    }
  }
  auto addr = get_rodata_addr(ib, regmap);

  unsigned origin_len = 0;
  sloejit::reg r;

  if (v->type->nelems.is_sve()) {
    ASSERT(HaveSVE);
    ASSERT(pred);
    int ofs2 = ofs;
    if (ofs2 >= 128) {
      addr = ib.make_x_add_ri(addr, ofs2);
      ++origin_len;
      ofs2 = 0;
    }
    switch (v->type->elem_width) {
    case 8:
      r = ib.make_ld1rqb_pri(*pred, addr, ofs2);
      break;
    case 16:
      r = ib.make_ld1rqh_pri(*pred, addr, ofs2);
      break;
    case 32:
      r = ib.make_ld1rqw_pri(*pred, addr, ofs2);
      break;
    case 64:
      r = ib.make_ld1rqd_pri(*pred, addr, ofs2);
      break;
    case 128:
      r = ib.make_ld1rqd_pri(*pred, addr, ofs2);
      break;
    default:
      ASSERT(false);
    }
  } else {
    ASSERT(v->type->nelems.is_contig());
    int segment_width = v->type->nelems.count_contig() * v->type->elem_width;
    switch (segment_width) {
    case 8: {
      r = ib.make_b_ldr_ri(addr, ofs);
      break;
    }
    case 16: {
      r = ib.make_h_ldr_ri(addr, ofs);
      break;
    }
    case 32: {
      r = ib.make_s_ldr_ri(addr, ofs);
      break;
    }
    case 64: {
      r = ib.make_d_ldr_ri(addr, ofs);
      break;
    }
    case 128: {
      r = ib.make_q_ldr_ri(addr, ofs);
      break;
    }
    default:
      ASSERT(false);
    }
  }
  ++origin_len;
  data_ofs.push_back(rodata_info{
      .val = val.data,
      .ofs = ofs,
      .r = r,
      .nelems = v->type->nelems,
      .origin = ib.get_last_inserted_instr(),
      .origin_len = origin_len,
  });
  return r;
}

} // end namespace plfft::wfta
