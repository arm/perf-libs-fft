/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "io_pointers.hpp"
#include "irbuilder.hpp"
#include "plfft.h"
#include "plfft/algo_flops.hpp"
#include "plfft_assert.hpp"
#include "plfft_kernels.hpp"
#include "rtype.hpp"
#include "target.hpp"

#include <map>
#include <vector>

namespace plfft::wfta {

enum type_kind { TK_NONE, TK_REAL, TK_IMAG, TK_COMPLEX };

struct expr_type {
  type_kind kind;

  expr_type() : kind(TK_NONE) {}

  expr_type(type_kind kind) : kind(kind) {
    ASSERT(kind != TK_NONE);
  }

  inline bool operator==(const expr_type &r) const {
    return kind == r.kind;
  }

  inline bool operator!=(const expr_type &r) const {
    return !(*this == r);
  }
};

// Indicator for type of 2nd ("right") operand in expression
enum rt { RT_NONE, RT_REAL_CONST, RT_IMAG_CONST, RT_PTR, RT_WPTR };

// Literals are calculated based on sin/cos calls plus floating-point
// arithmetic and this means that the values produced are inexact, so we have a
// margin below which we will consider values to be identical (such that they
// can be merged together to avoid duplicate literals being materialised
// twice).
constexpr double RVAL_APPROX_MARGIN = 0.00001;

struct atom {
  enum rt kind;

  union {
    double rval;
    size_t ival;
  };

  expr_type self_type;

  atom() : kind(RT_NONE) {}

  atom(enum rt kind, size_t val) : kind(kind), ival(val) {
    ASSERT(kind == RT_PTR || kind == RT_WPTR);
  }

  atom(enum rt kind, double val) : kind(kind), rval(val) {
    ASSERT(kind == RT_REAL_CONST || kind == RT_IMAG_CONST);
  }

  atom(enum rt kind) : kind(kind) {
    ASSERT(kind == RT_NONE);
  }

  inline bool operator==(const atom &r) const {
    if (kind != r.kind) {
      return false;
    }
    switch (kind) {
    case RT_REAL_CONST:
    case RT_IMAG_CONST:
      return r.rval == rval;
    case RT_WPTR:
    case RT_PTR:
      return r.ival == ival;
    case RT_NONE:
      return true;
    }
    return false;
  }

  inline bool operator!=(const atom &r) const {
    return !(*this == r);
  }

  inline bool operator<(const atom &r) const {
    // it doesn't make sense to compare completely different or null atoms
    if (kind != r.kind) {
      return false;
    }
    switch (kind) {
    case RT_REAL_CONST:
    case RT_IMAG_CONST:
      return rval < r.rval;
    case RT_WPTR:
    case RT_PTR:
      return ival < r.ival;
    case RT_NONE:
      return true;
    }
    return false;
  }
};

/** A helper class to generate fresh local variables. */
class fresh_atom_factory {
  size_t next_id;

public:
  fresh_atom_factory(size_t start_id = 0) : next_id(start_id) {}

  atom operator()() {
    return atom(RT_PTR, ++next_id);
  }

  std::vector<atom> get_many(int n) {
    std::vector<atom> ret;
    for (int i = 0; i < n; ++i) {
      ret.emplace_back(RT_PTR, ++next_id);
    }
    return ret;
  }
};

typedef struct expr {
  atom lhs;
  atom left;
  char op;
  atom right;
  expr_type op_type;

public:
  expr(atom lhs, atom left, char op, atom right)
    : lhs(lhs), left(left), op(op), right(right) {}

  expr(atom lhs, atom left) : lhs(lhs), left(left), op('\0'), right(RT_NONE) {}

  void print(ir_builder &b, const target_t &t,
             const std::map<std::string, ir_value> &params,
             std::map<atom, ir_value> &vals, int64_t n, const io_ptr_t &iop,
             const std::vector<ir_value> &in_vals,
             std::vector<ir_value> &out_vals, plfft_direction_t dir,
             const kernel_types_t &types, order_kind order,
             const io_mods_t &mods) const;

  inline bool operator==(const expr &other) const {
    return lhs == other.lhs && left == other.left && op == other.op &&
           right == other.right;
  }

  inline bool operator<(const expr &other) const {
    return lhs < other.lhs;
  }

  void debug_print() const;
  algo_flops flops() const;

} expr_t;

} // namespace plfft::wfta
