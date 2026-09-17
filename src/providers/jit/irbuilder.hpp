/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irvalue.hpp"
#include "irvalue_scope.hpp"
#include "rtype.hpp"

namespace plfft::wfta {

struct ir_builder {
  ir_value_scope *scope;

  const target_t &target;
  register_layout layout;

  ir_builder(ir_value_scope *scope, const target_t &target,
             register_layout layout)
    : scope(scope), target(target), layout(layout) {}

  ir_builder get_builder_for_scope(ir_value_scope *new_scope) const {
    return {new_scope, target, layout};
  }

  inline ir_value_num_elems nelems_max(ir_value_num_elems a) {
    return a;
  }

  template<typename... Ts>
  inline ir_value_num_elems nelems_max(ir_value_num_elems a,
                                       ir_value_num_elems b, Ts... cs) {
    return nelems_max(a.max(b), cs...);
  }

  /// build an integer constant of given width
  ir_value build_int_constant(int width, int64_t val);
  /// build a real constant of type `t` (floating-point or fixed-point)
  ir_value build_real_constant(ir_value_type_ptr t, double val);
  /// build a complex constant of type `t` (floating-point or fixed-point)
  ir_value build_complex_constant(ir_value_type_ptr t, double re, double im);

  ir_value build_vec_from_elems(std::vector<ir_value> elems);
  ir_value build_splat_if_needed(ir_value a, ir_value_num_elems nelems);

  /// [a], [b] -> [a, b]
  ir_value build_complex_from_elems(ir_value a, ir_value b);

  ir_value build_take_vec_elem(ir_value a, int lane);

  /// [r, r]
  ir_value build_splat_real(ir_value a);

  /// [i, i]
  ir_value build_splat_imag(ir_value a);

  ir_value build_take_real(ir_value a);

  /// [a, b] -> [0, a]
  ir_value build_sve_promote_to_imag_complex(ir_value a);

  ir_value build_take_imag(ir_value a);

  ir_value build_uzp1(ir_value a, ir_value b, ir_value_type_ptr t,
                      int elem_width);
  ir_value build_uzp2(ir_value a, ir_value b, ir_value_type_ptr t,
                      int elem_width);

  /// [a, b, c, d] -> [d, c, b, a]
  ir_value build_rev_vec(ir_value a);

  /// [a, b, c, d] -> [b, a, d, c]
  ir_value build_rev_pairs(ir_value a);

  /// [a, b] -> [-b, a]
  ir_value build_make_imag(ir_value a);

  /// a -> 0 - a
  ir_value build_ineg(ir_value a);

  ir_value build_iadd(ir_value a, ir_value b);
  ir_value build_isub(ir_value a, ir_value b);
  ir_value build_imul(ir_value a, ir_value b);
  ir_value build_idiv(ir_value a, ir_value b);
  ir_value build_imod(ir_value a, ir_value b);

  // add/sub/mul for floating-point or fixed-point values
  ir_value build_add(ir_value a, ir_value b);
  ir_value build_sub(ir_value a, ir_value b);
  ir_value build_mul(ir_value a, ir_value b);

  /// dispatch to appropriate floating-point or fixed-point binop builder
  ir_value build_binop(ir_value a, char op, ir_value b);
  ir_value build_binop(ir_value a, ir_value_op op, ir_value b);

  /// conjugate a floating-point or fixed-point complex value
  ir_value build_conj(ir_value a);

  /// negate a floating-point or fixed-point value
  ir_value build_neg(ir_value a);

  /// build a complex floating-point or fixed-point value from a real one
  /// [a] -> [a, 0]
  ir_value build_make_complex(ir_value a);

  /// signed rounding right shift
  ir_value build_srshr(ir_value a, unsigned shift);

  /// &a[b]
  ir_value build_gep(ir_value a, ir_value b);

  /// upcast/downcast to element size of n bits
  ir_value build_cast(ir_value a, int nbits);

  /// a[b]
  ir_value build_gather(ir_value a, ir_value b);
  ir_value build_gather(ir_value a, ir_value b, ir_value_type_ptr t);
  ir_value build_load(ir_value a, ir_value b);
  ir_value build_load(ir_value a, ir_value b, ir_value_type_ptr t);
  ir_value build_load_or_gather(ir_value a, ir_value b);
  ir_value build_load_or_gather(ir_value a, ir_value b, ir_value_type_ptr t);

  // Duplicating load
  ir_value build_load_bcast(ir_value a, ir_value b, ir_value_type_ptr t);

  /// ptrue (element width is always treated as bytes).
  ir_value build_ptrue();

  /// whilelt(start, end) with the given element width.
  ir_value build_whilelt(ir_value a, ir_value b, int elem_width);

  /// a[b], where both a and b are scalar
  ir_value build_predicated_load(ir_value pred, ir_value a, ir_value b,
                                 ir_value_type_ptr t);
  std::vector<ir_value> build_sve_structure_load(int n, ir_value pred,
                                                 ir_value a, ir_value b,
                                                 ir_value_type_ptr t);
  std::vector<ir_value> build_structure_load(int n, ir_value a, ir_value b,
                                             ir_value_type_ptr t);

  /// b[c] = a, where a is a scalar or Neon or SVE vector and
  ///                 c is a scalar or vector of integral type
  void build_scatter(ir_value a, ir_value b, ir_value c);
  void build_store(ir_value a, ir_value b, ir_value c);
  void build_sve_structure_store(const std::vector<ir_value> &values,
                                 ir_value pred, ir_value a, ir_value b);
  void build_structure_store(const std::vector<ir_value> &values, ir_value a,
                             ir_value b);
  void build_store_or_scatter(ir_value a, ir_value b, ir_value c);

  /// [a, a+b, a+b+b, ...]
  ir_value build_index(ir_value a, ir_value b, ir_value_num_elems nelems);

  /// a == b ? c : d
  ir_value build_eq_sel(ir_value a, ir_value b, ir_value c, ir_value d);

  /// min(a, b)
  ir_value build_min(ir_value a, ir_value b);

  /// a = [a, b, c, d, ...], b = [e, f, g, h, ...] -> [a, e, b, f, ...]
  ir_value build_zip(ir_value a, ir_value b);
  ir_value build_zip1(ir_value a, ir_value b, ir_value_type_ptr t,
                      int elem_width);
  ir_value build_zip2(ir_value a, ir_value b, ir_value_type_ptr t,
                      int elem_width);

  /// perform a complex multiplication on floating-point or fixed-point values
  /// [a, b], [c, d] -> [a*c - b*d, b*c + a*d]
  ir_value build_cmul(ir_value a, ir_value b);

  /// svcnth()
  ir_value build_sve_cnth();

  /// svcntw()
  ir_value build_sve_cntw();

  /// svcntd()
  ir_value build_sve_cntd();

  /// Create an SSA token representing zeroed ZA tile.
  ir_value build_za_zero();

  /// Write a vector slice to ZA and return the updated ZA tile.
  ir_value build_za_slice_write(ir_value za, ir_value pred, ir_value vec,
                                int plfft_direction_t, int tile,
                                ir_value slice);

  /// Read a vector slice from ZA using pred_full.
  ir_value build_za_slice_read(ir_value za, ir_value_type_ptr t,
                               int plfft_direction_t, int tile, ir_value slice);

  // [a, b, c, d, ...] -> [a, c, ...]
  ir_value build_sve_take_neon_real(ir_value a);
};

} // namespace plfft::wfta
