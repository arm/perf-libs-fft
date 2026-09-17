/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irvalue.hpp"
#include "plfft_assert.hpp"

namespace plfft::wfta {

static inline bool is_dup(ir_value v) {
  if (v->op != IVO_SHUFFLE) {
    return false;
  }
  auto indices = v->literals;
  ASSERT(!indices.empty());
  int id = (int)indices[0];
  for (unsigned i = 1; i < indices.size(); ++i) {
    if ((int)indices[i] != id) {
      return false;
    }
  }
  return true;
}

static inline bool is_rev_pairs(ir_value v) {
  auto indices = v->literals;
  ASSERT(!indices.empty());
  if (indices.size() % 2 != 0) {
    return false;
  }
  for (unsigned i = 0; i < indices.size(); i += 2) {
    if ((unsigned)indices[i] != i + 1 || (unsigned)indices[i + 1] != i) {
      return false;
    }
  }
  return true;
}

static inline bool is_rev_vec(ir_value v) {
  auto indices = v->literals;
  ASSERT(!indices.empty());
  if (indices.size() % 2 != 0) {
    return false;
  }
  for (unsigned i = 0; i < indices.size(); ++i) {
    if ((unsigned)indices[i] != indices.size() - i - 1) {
      return false;
    }
  }
  return true;
}

static inline bool is_trn(ir_value v) {
  if (v->op != IVO_SHUFFLE) {
    return false;
  }
  auto indices = v->literals;
  ASSERT(!indices.empty());
  if (indices.size() % 2 != 0) {
    return false;
  }
  int id = (int)indices[0]; // either 0 or 1
  if (id != 0 && id != 1) {
    return false;
  }
  for (unsigned i = 0; i < indices.size(); ++i) {
    if ((unsigned)indices[i] != i / 2 + id) {
      return false;
    }
  }
  return true;
}

static inline bool is_lane_get(ir_value v) {
  return v->op == IVO_SHUFFLE && v->literals.size() == 1;
}

static inline bool is_neon_take_real(ir_value v) {
  if (v->op != IVO_SHUFFLE) {
    return false;
  }
  if (v->literals.size() == 1) {
    return false;
  }
  for (unsigned i = 0; i < v->literals.size(); ++i) {
    if (v->literals[i] != (double)i * 2) {
      return false;
    }
  }
  return true;
}

static inline bool is_neon_take_imag(ir_value v) {
  if (v->op != IVO_SHUFFLE) {
    return false;
  }
  if (v->literals.size() == 1) {
    return false;
  }
  for (unsigned i = 0; i < v->literals.size(); ++i) {
    if (v->literals[i] != (double)i * 2 + 1) {
      return false;
    }
  }
  return true;
}

static inline bool is_all_irvalue_equal(const std::vector<ir_value> &val) {
  ASSERT(!val.empty());
  for (size_t i = 1; i < val.size(); ++i) {
    if (val[i] != val[0]) {
      return false;
    }
  }
  return true;
}

static inline bool want_delayed_gep(ir_value v) {
  ASSERT(v->op == IVO_GEP);
  ASSERT(v->type->nelems.is_one());
  if (v->uses.size() != 1) {
    return false;
  }
  auto *u = v->uses[0].v;
  ASSERT(u);
  return is_generic_load(u->op) || u->op == IVO_SCATTER || u->op == IVO_STORE ||
         u->op == IVO_STRUCTURE_STORE_GROUP;
}

} // end namespace plfft::wfta
