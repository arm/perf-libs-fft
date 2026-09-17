/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "irbuilder.hpp"
#include "plfft_assert.hpp"

#include <iostream>

namespace plfft::wfta {

static inline ir_value_op select_float_or_fixed_op(ir_value_type_ptr t,
                                                   ir_value_op float_op,
                                                   ir_value_op fixed_op) {
  switch (t->kind) {
  case IVK_FLOAT:
    return float_op;
  case IVK_FIXED:
    return fixed_op;
  default:
    std::cerr << "invalid real type: (" << (int)t->kind << ")" << std::endl;
    ASSERT(false);
  }
}

ir_value ir_builder::build_int_constant(int width, int64_t val) {
  return scope->create_ir_value(
      IVO_CONST_INT, make_ir_value_type_integer(width), {}, {(double)val}, "");
}

ir_value ir_builder::build_real_constant(ir_value_type_ptr t, double val) {
  switch (t->kind) {
  case IVK_FLOAT:
    return scope->create_ir_value(IVO_CONST_FLOAT,
                                  make_ir_value_type_real(t->elem_width, true),
                                  {}, {val}, "");
  case IVK_FIXED:
    return scope->create_ir_value(IVO_CONST_FIXED,
                                  make_ir_value_type_real(t->elem_width, false),
                                  {}, {val}, "");
  default:
    std::cerr << "invalid real type: (" << (int)t->kind << ")" << std::endl;
    ASSERT(false);
  }
}

ir_value ir_builder::build_complex_constant(ir_value_type_ptr t, double re,
                                            double im) {
  auto re2 = build_real_constant(t, re);
  auto im2 = build_real_constant(t, im);
  return build_vec_from_elems({re2, im2});
}

ir_value ir_builder::build_vec_from_elems(std::vector<ir_value> elems) {
  ASSERT(!elems.empty());
  if (elems.size() == 1) {
    return elems[0];
  }
  auto elem_t = make_ir_value_type_novector(elems[0]->type);
  auto nelems = elems[0]->type->nelems;
  for (unsigned i = 1; i < elems.size(); ++i) {
    nelems = ir_value_num_elems::concat(nelems, elems[i]->type->nelems);
  }
  auto vec_t = make_ir_value_type_vector(nelems, elem_t->elem_width, elem_t);
  return scope->create_ir_value(IVO_CONCAT, std::move(vec_t), std::move(elems),
                                {}, "");
}

ir_value ir_builder::build_splat_if_needed(ir_value a,
                                           ir_value_num_elems nelems) {
  ASSERT(nelems.max(a->type->nelems) == nelems);
  ASSERT(!a->type->nelems.is_sve() || nelems.is_sve());
  if (a->type->nelems == nelems) {
    return a;
  }
  auto vec_t = make_ir_value_type_vector(nelems, a->type->elem_width,
                                         make_ir_value_type_novector(a->type));
  return scope->create_ir_value(IVO_CONCAT, std::move(vec_t), {a}, {}, "");
}

ir_value ir_builder::build_take_vec_elem(ir_value a, int lane) {
  ASSERT(!a->type->nelems.is_sve());
  ASSERT(lane >= 0);
  ASSERT((a->type->nelems.segment_mask & (1 << lane)) != 0);
  auto t = make_ir_value_type_novector(a->type);
  return scope->create_ir_value(IVO_SHUFFLE, std::move(t), {a}, {(double)lane},
                                "");
}

/// [r, r]
ir_value ir_builder::build_splat_real(ir_value a) {
  std::vector<double> indices;
  for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
    int live = (a->type->nelems.segment_mask >> i) & 0b11;
    ASSERT(live == 0 || live == 0b11);
    if (live != 0) {
      indices.push_back(i / 2);
      indices.push_back(i / 2);
    }
  }
  return scope->create_ir_value(IVO_SHUFFLE, a->type, {a}, std::move(indices),
                                "");
}

/// [i, i]
ir_value ir_builder::build_splat_imag(ir_value a) {
  std::vector<double> indices;
  for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
    int live = (a->type->nelems.segment_mask >> i) & 0b11;
    ASSERT(live == 0 || live == 0b11);
    if (live != 0) {
      indices.push_back(i / 2 + 1);
      indices.push_back(i / 2 + 1);
    }
  }
  return scope->create_ir_value(IVO_SHUFFLE, a->type, {a}, std::move(indices),
                                "");
}

ir_value ir_builder::build_take_real(ir_value a) {
  if (a->type->nelems.is_sve()) {
    int new_mask = 0;
    for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
      int live = (a->type->nelems.segment_mask >> i) & 0b11;
      ASSERT(live == 0 || live == 0b11);
      if (live != 0) {
        new_mask |= 1 << i;
      }
    }
    auto zero =
        build_splat_if_needed(build_real_constant(a->type, 0), a->type->nelems);
    auto t = make_ir_value_type_novector(a->type);
    t = make_ir_value_type_vector(
        ir_value_num_elems::with_mask(new_mask, a->type->nelems.scale),
        a->type->elem_width, std::move(t));
    // TODO: consider if you can remove the zero param here?
    return scope->create_ir_value(IVO_SVE_TRN1, std::move(t), {a, zero}, {},
                                  "");
  }
  std::vector<ir_value> elems;
  for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
    int live = (a->type->nelems.segment_mask >> i) & 0b11;
    ASSERT(live == 0 || live == 0b11);
    if (live != 0) {
      elems.push_back(build_take_vec_elem(a, i));
    }
  }
  return build_vec_from_elems(std::move(elems));
}

ir_value ir_builder::build_sve_take_neon_real(ir_value a) {
  ASSERT(a->type->nelems.is_sve());
  int new_mask = 0;
  for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
    int live = (a->type->nelems.segment_mask >> i) & 0b11;
    ASSERT(live == 0 || live == 0b11);
    if (live != 0) {
      new_mask |= 1 << (i / 2);
    }
  }
  auto t = make_ir_value_type_novector(a->type);
  t = make_ir_value_type_vector(
      ir_value_num_elems::with_mask(new_mask, a->type->nelems.scale),
      a->type->elem_width, std::move(t));
  return scope->create_ir_value(IVO_SHUFFLE, std::move(t), {a},
                                std::vector<double>{0.0, 2.0}, "");
}

ir_value ir_builder::build_sve_promote_to_imag_complex(ir_value a) {
  ASSERT(a->type->nelems.is_sve());
  int new_mask = 0;
  for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
    int live = (a->type->nelems.segment_mask >> i) & 0b11;
    ASSERT(live == 0b0 || live == 0b1);
    if (live != 0) {
      new_mask |= 0b11 << i;
    }
  }
  // build a vector filled with zeroes
  auto t = make_ir_value_type_novector(a->type);
  t = make_ir_value_type_vector(
      ir_value_num_elems::with_mask(new_mask, a->type->nelems.scale),
      a->type->elem_width, std::move(t));
  auto zero =
      build_splat_if_needed(build_real_constant(a->type, 0), a->type->nelems);
  return scope->create_ir_value(IVO_SVE_TRN1, std::move(t), {zero, a}, {}, "");
}

ir_value ir_builder::build_take_imag(ir_value a) {
  if (a->type->nelems.is_sve()) {
    int new_mask = 0;
    for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
      int live = (a->type->nelems.segment_mask >> i) & 0b11;
      ASSERT(live == 0 || live == 0b11);
      if (live != 0) {
        new_mask |= 1 << i;
      }
    }
    auto zero =
        build_splat_if_needed(build_real_constant(a->type, 0), a->type->nelems);
    auto t = make_ir_value_type_novector(a->type);
    t = make_ir_value_type_vector(
        ir_value_num_elems::with_mask(new_mask, a->type->nelems.scale),
        a->type->elem_width, std::move(t));
    // TODO: consider if you can remove the zero param here?
    return scope->create_ir_value(IVO_SVE_TRN2, std::move(t), {a, zero}, {},
                                  "");
  }
  std::vector<ir_value> elems;
  for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
    int live = (a->type->nelems.segment_mask >> i) & 0b11;
    ASSERT(live == 0 || live == 0b11);
    if (live != 0) {
      elems.push_back(build_take_vec_elem(a, i + 1));
    }
  }
  return build_vec_from_elems(std::move(elems));
}

ir_value ir_builder::build_uzp1(ir_value a, ir_value b, ir_value_type_ptr t,
                                int elem_width) {
  ASSERT(a->type->nelems.is_sve() == b->type->nelems.is_sve());
  ASSERT(a->type->nelems.is_sve() == t->nelems.is_sve());
  return scope->create_ir_value(IVO_UZP1, std::move(t), {a, b},
                                {(double)elem_width}, "");
}

ir_value ir_builder::build_uzp2(ir_value a, ir_value b, ir_value_type_ptr t,
                                int elem_width) {
  ASSERT(a->type->nelems.is_sve() == b->type->nelems.is_sve());
  ASSERT(a->type->nelems.is_sve() == t->nelems.is_sve());
  return scope->create_ir_value(IVO_UZP2, std::move(t), {a, b},
                                {(double)elem_width}, "");
}

/// [a, b, c, d] -> [d, c, b, a]
ir_value ir_builder::build_rev_vec(ir_value a) {
  ASSERT(!a->type->nelems.is_sve());
  ASSERT(a->type->nelems.is_contig());
  std::vector<double> indices;
  int count = a->type->nelems.count_contig();
  for (int i = 0; i < count; ++i) {
    indices.push_back(count - i - 1);
  }
  return scope->create_ir_value(IVO_SHUFFLE, a->type, {a}, std::move(indices),
                                "");
}

/// [a, b, c, d] -> [b, a, d, c]
ir_value ir_builder::build_rev_pairs(ir_value a) {
  ASSERT(a->type->nelems.is_contig());
  ASSERT(a->type->nelems.is_even());
  std::vector<double> indices;
  for (int i = 0; i < a->type->nelems.count_contig(); i += 2) {
    indices.push_back(i + 1);
    indices.push_back(i);
  }
  return scope->create_ir_value(IVO_SHUFFLE, a->type, {a}, std::move(indices),
                                "");
}

ir_value ir_builder::build_make_imag(ir_value a) {
  return build_rev_pairs(build_conj(a));
}

ir_value ir_builder::build_mul(ir_value a, ir_value b) {
  auto op = select_float_or_fixed_op(a->type, IVO_FMUL, IVO_SQMUL);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  return scope->create_ir_value(op, a->type, {a, b}, {}, "");
}

ir_value ir_builder::build_imul(ir_value a, ir_value b) {
  ASSERT(a->type->elem_width == b->type->elem_width);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  auto t = select_widest_ir_value_type_integer(a->type, b->type);
  return scope->create_ir_value(IVO_IMUL, t, {a, b}, {}, "");
}

ir_value ir_builder::build_idiv(ir_value a, ir_value b) {
  ASSERT(a->type->elem_width == b->type->elem_width);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  auto t = select_widest_ir_value_type_integer(a->type, b->type);
  return scope->create_ir_value(IVO_IDIV, t, {a, b}, {}, "");
}

ir_value ir_builder::build_imod(ir_value a, ir_value b) {
  ASSERT(a->type->elem_width == b->type->elem_width);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  auto t = select_widest_ir_value_type_integer(a->type, b->type);
  return scope->create_ir_value(IVO_IMOD, t, {a, b}, {}, "");
}

ir_value ir_builder::build_add(ir_value a, ir_value b) {
  auto op = select_float_or_fixed_op(a->type, IVO_FADD, IVO_SQADD);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  return scope->create_ir_value(op, a->type, {a, b}, {}, "");
}

ir_value ir_builder::build_iadd(ir_value a, ir_value b) {
  ASSERT(a->type->elem_width == b->type->elem_width);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  auto t = select_widest_ir_value_type_integer(a->type, b->type);
  return scope->create_ir_value(IVO_IADD, t, {a, b}, {}, "");
}

ir_value ir_builder::build_sub(ir_value a, ir_value b) {
  auto op = select_float_or_fixed_op(a->type, IVO_FSUB, IVO_SQSUB);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  return scope->create_ir_value(op, a->type, {a, b}, {}, "");
}

ir_value ir_builder::build_ineg(ir_value a) {
  return build_isub(build_int_constant(a->type->elem_width, 0), a);
}

ir_value ir_builder::build_isub(ir_value a, ir_value b) {
  ASSERT(a->type->elem_width == b->type->elem_width);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  auto t = select_widest_ir_value_type_integer(a->type, b->type);
  return scope->create_ir_value(IVO_ISUB, t, {a, b}, {}, "");
}

ir_value ir_builder::build_binop(ir_value a, char op, ir_value b) {
  switch (op) {
  case '+':
    return build_add(a, b);
  case '-':
    return build_sub(a, b);
  case '*':
    return build_mul(a, b);
  }
  std::cerr << "no such float operator: (" << (int)op << ") " << op
            << std::endl;
  ASSERT(false);
}

ir_value ir_builder::build_binop(ir_value a, ir_value_op op, ir_value b) {
  switch (op) {
  case IVO_FADD:
  case IVO_SQADD:
    return build_add(a, b);
  case IVO_FSUB:
  case IVO_SQSUB:
    return build_sub(a, b);
  case IVO_FMUL:
  case IVO_SQMUL:
    return build_mul(a, b);
  default:
    std::cerr << "no such operator: (" << (int)op << ")" << std::endl;
    ASSERT(false);
  }
}

ir_value ir_builder::build_conj(ir_value a) {
  // SVE has predicated negation which makes this easy, but we can also
  // emulate this in non-fp64 floating-point by negating at double-width.
  if (target.has_sve ||
      (a->type->kind == IVK_FLOAT && a->type->elem_width <= 32)) {
    return scope->create_ir_value(IVO_FCONJ, a->type, {a}, {}, "");
  }
  auto conj_mul_const = build_complex_constant(a->type, 1.0, -1.0);
  return build_mul(a, conj_mul_const);
}

ir_value ir_builder::build_neg(ir_value a) {
  auto op = select_float_or_fixed_op(a->type, IVO_FNEG, IVO_SQNEG);
  return scope->create_ir_value(op, a->type, {a}, {}, "");
}

ir_value ir_builder::build_make_complex(ir_value a) {
  ASSERT(is_float_or_fixed_kind(a->type->kind));
  auto zero = build_real_constant(a->type, 0);
  auto zero_vec = build_splat_if_needed(zero, a->type->nelems);
  auto t = make_ir_value_type_novector(a->type);
  if (a->type->nelems.is_sve()) {
    int new_mask = 0;
    for (int i = 0; (a->type->nelems.segment_mask >> i) != 0; i += 2) {
      int live = (a->type->nelems.segment_mask >> i) & 0b11;
      ASSERT(live == 0 || live == 0b1);
      if (live != 0) {
        new_mask |= 0b11 << i;
      }
    }
    t = make_ir_value_type_vector(
        ir_value_num_elems::with_mask(new_mask, a->type->nelems.scale),
        a->type->elem_width, std::move(t));
    return scope->create_ir_value(IVO_SVE_TRN1, std::move(t), {a, zero_vec}, {},
                                  "");
  }
  ASSERT(a->type->nelems.is_contig());
  if (a->type->nelems.count_contig() == 1) {
    // TO DO: This can be
    // scope->create_ir_value(IVO_CONCAT, std::move(vec_t), { a }, {}, "");
    // which is a mov (which is actually not needed?).
    return build_vec_from_elems({a, zero});
  }
  int new_mask = 0;
  for (int i = 0; i < a->type->nelems.count_contig(); ++i) {
    int live = a->type->nelems.segment_mask & (1 << i);
    if (live != 0) {
      new_mask |= 0b11 << (2 * i);
    }
  }
  // We treat the zero vector and the vector a as 128-bit wide when zipping.
  // We do not care about the upper half of the zero vector and the vector a.
  t = make_ir_value_type_vector(ir_value_num_elems::with_mask(new_mask),
                                a->type->elem_width, std::move(t));
  return scope->create_ir_value(IVO_ZIP1, std::move(t), {a, zero_vec}, {}, "");
}

ir_value ir_builder::build_srshr(ir_value a, unsigned shift) {
  ASSERT(a->type->kind == IVK_FIXED);
  return scope->create_ir_value(IVO_SRSHR, a->type, {a}, {(double)shift}, "");
}

/// &a[b]
ir_value ir_builder::build_gep(ir_value a, ir_value b) {
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());
  return scope->create_ir_value(IVO_GEP, a->type, {a, b}, {}, "");
}

/** upcast/downcast to element size of n bits */
ir_value ir_builder::build_cast(ir_value a, int nbits) {
  auto t = make_ir_value_type_vector(a->type->nelems, a->type->elem_width,
                                     make_ir_value_type_real(nbits, true));
  return scope->create_ir_value(IVO_CAST, std::move(t), {a}, {}, "");
}

/// a[b]
ir_value ir_builder::build_gather(ir_value a, ir_value b) {
  ASSERT(a->type->inner_type);
  ASSERT(a->type->nelems.is_one());
  ASSERT(!b->type->nelems.is_one());
  auto ty = make_ir_value_type_vector_for_load(target, layout, b->type->nelems,
                                               b->type->elem_width,
                                               a->type->inner_type);
  return build_gather(a, b, ty);
}

/// a[b]
ir_value ir_builder::build_gather(ir_value a, ir_value b, ir_value_type_ptr t) {
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(!b->type->nelems.is_one());
  return scope->create_ir_value(IVO_GATHER, std::move(t), {a, b}, {}, "");
}

/// a[b], contiguous load where both a and b are scalar
ir_value ir_builder::build_load(ir_value a, ir_value b) {
  ASSERT(a->type->inner_type);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->nelems.is_one());
  auto ty = make_ir_value_type_vector_for_load(target, layout, b->type->nelems,
                                               b->type->elem_width,
                                               a->type->inner_type);
  return build_load(a, b, ty);
}

/// a[b], contiguous load where both a and b are scalar
ir_value ir_builder::build_load(ir_value a, ir_value b, ir_value_type_ptr t) {
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());
  return scope->create_ir_value(IVO_LOAD, std::move(t), {a, b}, {}, "");
}

ir_value ir_builder::build_load_or_gather(ir_value a, ir_value b,
                                          ir_value_type_ptr t) {
  if (b->type->nelems.is_one()) {
    return build_load(a, b, t);
  } else {
    return build_gather(a, b, t);
  }
}

ir_value ir_builder::build_load_or_gather(ir_value a, ir_value b) {
  auto ty = make_ir_value_type_vector_for_load(target, layout, b->type->nelems,
                                               b->type->elem_width,
                                               a->type->inner_type);
  return build_load_or_gather(a, b, ty);
}

ir_value ir_builder::build_load_bcast(ir_value a, ir_value b,
                                      ir_value_type_ptr t) {
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(b->type->kind == IVK_INTEGER);
  auto inner_type = a->type->inner_type;
  ASSERT(inner_type->nelems.is_contig());
  int segment_width =
      inner_type->nelems.count_contig() * inner_type->elem_width;
  ASSERT(segment_width == 16 || segment_width == 32 || segment_width == 64 ||
         segment_width == 128);
  ir_value_type_ptr load_type = t;
  if (!t->nelems.is_sve() && t->nelems.is_contig()) {
    int full_lanes = 128 / t->elem_width;
    ASSERT(full_lanes > 0 && full_lanes <= 16);
    if (t->nelems.count_contig() != full_lanes) {
      auto full_nelems = ir_value_num_elems::with_nlanes(full_lanes);
      load_type = std::make_shared<ir_value_type>(t->kind, t->elem_width,
                                                  full_nelems, t->inner_type);
    }
  }
  auto load_val = scope->create_ir_value(IVO_LOAD_BCAST, std::move(load_type),
                                         {a, b}, {(double)segment_width}, "");
  if (*load_val->type == *t) {
    return load_val;
  }
  return scope->create_ir_value(IVO_REINTERPRET, std::move(t), {load_val}, {},
                                "");
}

ir_value ir_builder::build_ptrue() {
  auto t = make_ir_value_type_predicate(8);
  return scope->create_ir_value(IVO_PTRUE, std::move(t), {}, {}, "");
}

ir_value ir_builder::build_whilelt(ir_value a, ir_value b, int elem_width) {
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(b->type->kind == IVK_INTEGER);
  auto t = make_ir_value_type_predicate(elem_width);
  return scope->create_ir_value(IVO_WHILELT, std::move(t), {a, b}, {}, "");
}

/// a[b], contiguous load (SVE) where both a and b are scalar
ir_value ir_builder::build_predicated_load(ir_value pred, ir_value a,
                                           ir_value b, ir_value_type_ptr t) {
  ASSERT(pred->type->kind == IVK_PREDICATE);
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(t->nelems.is_sve());
  ASSERT(b->type->kind == IVK_INTEGER);
  return scope->create_ir_value(IVO_LOAD, std::move(t), {pred, a, b}, {}, "");
}

std::vector<ir_value>
ir_builder::build_sve_structure_load(int n, ir_value pred, ir_value a,
                                     ir_value b, ir_value_type_ptr t) {
  ASSERT(pred->type->kind == IVK_PREDICATE);
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(t->nelems.is_sve());
  ASSERT(b->type->kind == IVK_INTEGER);
  std::vector<ir_value> values(n);
  auto group =
      scope->create_ir_value(IVO_STRUCTURE_LOAD_GROUP, t, {pred, a, b}, {}, "");
  ASSERT((n == 2 || n == 3 || n == 4) &&
         "Unsupported structure load group width");
  for (int i = 0; i < n; i++) {
    values[i] =
        scope->create_ir_value(IVO_GET_GROUP_OP, t, {group}, {(double)i}, "");
  }
  return values;
}

std::vector<ir_value> ir_builder::build_structure_load(int n, ir_value a,
                                                       ir_value b,
                                                       ir_value_type_ptr t) {
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(t->nelems.is_contig() && t->count_contig_bits() == 128);
  ASSERT(b->type->kind == IVK_INTEGER);
  std::vector<ir_value> values(n);
  auto group =
      scope->create_ir_value(IVO_STRUCTURE_LOAD_GROUP, t, {a, b}, {}, "");
  ASSERT((n == 2 || n == 3 || n == 4) &&
         "Unsupported structure load group width");
  for (int i = 0; i < n; i++) {
    values[i] =
        scope->create_ir_value(IVO_GET_GROUP_OP, t, {group}, {(double)i}, "");
  }
  return values;
}

/// b[c] = a, where a is a scalar or Neon or SVE vector and c is a scalar or
/// vector of integral type
void ir_builder::build_scatter(ir_value a, ir_value b, ir_value c) {
  ASSERT(b->type->kind == IVK_POINTER);
  ASSERT(b->type->inner_type);
  ASSERT(c->type->kind == IVK_INTEGER);
  ASSERT(!c->type->nelems.is_one());
  // ASSERT(c->type->nelems == a->type->nelems); // currently not the case for
  // NEON
  scope->create_ir_value(IVO_SCATTER, make_ir_value_type_integer(64), {a, b, c},
                         {}, "");
}

/// b[c] = a, contiguous store where both b and c are scalar
void ir_builder::build_store(ir_value a, ir_value b, ir_value c) {
  ASSERT(b->type->kind == IVK_POINTER);
  ASSERT(b->type->inner_type);
  ASSERT(c->type->kind == IVK_INTEGER);
  ASSERT(c->type->nelems.is_one());
  scope->create_ir_value(IVO_STORE, make_ir_value_type_integer(64), {a, b, c},
                         {}, "");
}

void ir_builder::build_sve_structure_store(const std::vector<ir_value> &values,
                                           ir_value pred, ir_value a,
                                           ir_value b) {
  ASSERT(pred->type->kind == IVK_PREDICATE);
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());
  ASSERT((values.size() == 2 || values.size() == 3 || values.size() == 4) &&
         "Unsupported structure store group width");
  std::vector<ir_value> deps;
  deps.reserve(values.size() + 3);
  deps.push_back(pred);
  for (auto value : values) {
    ASSERT(value->type->nelems.is_sve());
    deps.push_back(value);
  }
  deps.push_back(a);
  deps.push_back(b);
  scope->create_ir_value(IVO_STRUCTURE_STORE_GROUP,
                         make_ir_value_type_integer(64), std::move(deps), {},
                         "");
}

void ir_builder::build_structure_store(const std::vector<ir_value> &values,
                                       ir_value a, ir_value b) {
  ASSERT(a->type->kind == IVK_POINTER);
  ASSERT(a->type->inner_type);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(b->type->nelems.is_one());
  ASSERT((values.size() == 2 || values.size() == 3 || values.size() == 4) &&
         "Unsupported structure store group width");
  std::vector<ir_value> deps;
  deps.reserve(values.size() + 2);
  for (auto value : values) {
    ASSERT(value->type->nelems.is_contig() &&
           value->type->count_contig_bits() == 128);
    deps.push_back(value);
  }
  deps.push_back(a);
  deps.push_back(b);
  scope->create_ir_value(IVO_STRUCTURE_STORE_GROUP,
                         make_ir_value_type_integer(64), std::move(deps), {},
                         "");
}

void ir_builder::build_store_or_scatter(ir_value a, ir_value b, ir_value c) {
  if (c->type->nelems.is_one()) {
    build_store(a, b, c);
  } else {
    build_scatter(a, b, c);
  }
}

/// [a, a+b, a+b+b, ...]
ir_value ir_builder::build_index(ir_value a, ir_value b,
                                 ir_value_num_elems nelems) {
  ASSERT(a->type->kind == IVK_INTEGER);
  ASSERT(b->type->kind == IVK_INTEGER);
  ASSERT(a->type->nelems.is_one());
  ASSERT(b->type->nelems.is_one());
  if (nelems.is_one()) {
    // if an index of length 1, it's just the base!
    return a;
  }
  auto t = make_ir_value_type_vector(nelems, a->type->elem_width, a->type);
  ASSERT(a->type->elem_width == b->type->elem_width);
  return scope->create_ir_value(IVO_INDEX, std::move(t), {a, b}, {}, "");
}

/// a == b ? c : d
ir_value ir_builder::build_eq_sel(ir_value a, ir_value b, ir_value c,
                                  ir_value d) {
  auto nelems = nelems_max(a->type->nelems, b->type->nelems, c->type->nelems,
                           d->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  c = build_splat_if_needed(c, nelems);
  d = build_splat_if_needed(d, nelems);
  ASSERT(*a->type == *b->type);
  ASSERT(*c->type == *d->type);
  return scope->create_ir_value(IVO_EQ_SEL, c->type, {a, b, c, d}, {}, "");
}

/// min(a, b)
ir_value ir_builder::build_min(ir_value a, ir_value b) {
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  ASSERT(*a->type == *b->type);
  return scope->create_ir_value(IVO_MIN, a->type, {a, b}, {}, "");
}

/// [a, b, c, d, ...] -> [a, a, b, b, ...]
ir_value ir_builder::build_zip(ir_value a, ir_value b) {
  ASSERT(*a->type == *b->type);
  return scope->create_ir_value(IVO_ZIP1, a->type, {a, b}, {}, "");
}

ir_value ir_builder::build_zip1(ir_value a, ir_value b, ir_value_type_ptr t,
                                int elem_width) {
  ASSERT(a->type->nelems.is_sve() == b->type->nelems.is_sve());
  ASSERT(a->type->nelems.is_sve() == t->nelems.is_sve());
  return scope->create_ir_value(IVO_ZIP1, std::move(t), {a, b},
                                {(double)elem_width}, "");
}

ir_value ir_builder::build_zip2(ir_value a, ir_value b, ir_value_type_ptr t,
                                int elem_width) {
  ASSERT(a->type->nelems.is_sve() == b->type->nelems.is_sve());
  ASSERT(a->type->nelems.is_sve() == t->nelems.is_sve());
  return scope->create_ir_value(IVO_ZIP2, std::move(t), {a, b},
                                {(double)elem_width}, "");
}

/// [a, b], [c, d] -> [a*c - b*d, b*c + a*d]
ir_value ir_builder::build_cmul(ir_value a, ir_value b) {
  auto op = select_float_or_fixed_op(a->type, IVO_FCMUL, IVO_SQCMUL);
  auto nelems = nelems_max(a->type->nelems, b->type->nelems);
  ASSERT(nelems.is_even());
  a = build_splat_if_needed(a, nelems);
  b = build_splat_if_needed(b, nelems);
  return scope->create_ir_value(op, a->type, {a, b}, {}, "");
}

/// svcnth()
ir_value ir_builder::build_sve_cnth() {
  return scope->create_ir_value(IVO_SVE_CNTH, make_ir_value_type_integer(64),
                                {}, {}, "");
}

/// svcntw()
ir_value ir_builder::build_sve_cntw() {
  return scope->create_ir_value(IVO_SVE_CNTW, make_ir_value_type_integer(64),
                                {}, {}, "");
}

/// svcntd()
ir_value ir_builder::build_sve_cntd() {
  return scope->create_ir_value(IVO_SVE_CNTD, make_ir_value_type_integer(64),
                                {}, {}, "");
}

ir_value ir_builder::build_za_zero() {
  auto t = std::make_shared<ir_value_type>(IVK_ZA_TILE, 0,
                                           ir_value_num_elems::one(), nullptr);
  return scope->create_ir_value(IVO_ZA_ZERO, std::move(t), {}, {}, "");
}

ir_value ir_builder::build_za_slice_write(ir_value za, ir_value pred,
                                          ir_value vec, int plfft_direction_t,
                                          int tile, ir_value slice) {
  ASSERT(za->type->kind == IVK_ZA_TILE);
  ASSERT(slice->type->kind == IVK_INTEGER);
  ASSERT(slice->type->nelems.is_one());
  ASSERT(pred->type->kind == IVK_PREDICATE);
  std::vector<double> literals = {(double)plfft_direction_t, (double)tile};
  return scope->create_ir_value(IVO_ZA_SLICE_WRITE, za->type,
                                {za, pred, vec, slice}, std::move(literals),
                                "");
}

ir_value ir_builder::build_za_slice_read(ir_value za, ir_value_type_ptr t,
                                         int plfft_direction_t, int tile,
                                         ir_value slice) {
  ASSERT(za->type->kind == IVK_ZA_TILE);
  ASSERT(slice->type->kind == IVK_INTEGER);
  ASSERT(slice->type->nelems.is_one());
  std::vector<double> literals = {(double)plfft_direction_t, (double)tile};
  return scope->create_ir_value(IVO_ZA_SLICE_READ, std::move(t), {za, slice},
                                std::move(literals), "");
}

} // end namespace plfft::wfta
