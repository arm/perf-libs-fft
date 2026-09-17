/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "type_check.hpp"
#include "plfft_assert.hpp"

namespace plfft::wfta {

/** Find the most-general type t such that t <= a and t <= b.
 * e.g. min(real, real) = real
 *      min(real, cmplx) = real
 *      min(real, imag) = 0
 */
static expr_type min_type(expr_type a, expr_type b) {
  ASSERT(a.kind != TK_NONE);
  ASSERT(b.kind != TK_NONE);
  ASSERT(!(a.kind == TK_REAL && b.kind == TK_IMAG));
  ASSERT(!(b.kind == TK_REAL && a.kind == TK_IMAG));
  return a.kind == TK_COMPLEX ? b : a;
}

/** find the most-specific type t such that a <= t and b <= t.
 *  e.g. max(real, real) = real
 *       max(real, cmplx) = cmplx
 *       max(real, imag) = cmplx
 */
static expr_type max_type(expr_type a, expr_type b) {
  ASSERT(a.kind != TK_NONE);
  ASSERT(b.kind != TK_NONE);
  if ((a.kind == TK_REAL && b.kind == TK_IMAG) ||
      (b.kind == TK_REAL && a.kind == TK_IMAG) || a.kind == TK_COMPLEX ||
      b.kind == TK_COMPLEX) {
    return TK_COMPLEX;
  }
  ASSERT(a == b);
  return a;
}

/** Find the conjugate type of a type:
 *  conj(real) = imag
 *  conj(imag) = real
 *  conj(cmplx) = cmplx
 */
static expr_type conj_type(expr_type a) {
  switch (a.kind) {
  case TK_NONE:
    ASSERT(false);
  case TK_REAL:
    return TK_IMAG;
  case TK_IMAG:
    return TK_REAL;
  case TK_COMPLEX:
    return TK_COMPLEX;
  }
  ASSERT(false);
}

/** Unify two types forwards through an additive operator.
 *  Given the types of the operator arguments, this yields the operator type and
 * result type. The operator type and result type are identical, and equal to
 * the most-specific common type of the operator arguments.
 */
static void unify_addsub_f(expr_type lhs, expr_type rhs, expr_type *op_type,
                           expr_type *res) {
  ASSERT(lhs.kind != TK_NONE);
  ASSERT(rhs.kind != TK_NONE);
  ASSERT(res);
  *res = *op_type = max_type(lhs, rhs);
}

/** Refine types backwards against an additive operator, all types should be set
 * on entry. If the operator or result types are narrower than they would
 * otherwise be from the operator argument types provided, we can narrow these
 * operator argument types to avoid doing unnecessary work. e.g. real = cmplx +
 * cmplx  ==>  real = real + real
 */
static void unify_addsub_b(expr_type *lhs, expr_type *rhs, expr_type *op_type,
                           expr_type res) {
  ASSERT(lhs);
  ASSERT(rhs);
  ASSERT(lhs->kind != TK_NONE);
  ASSERT(rhs->kind != TK_NONE);
  ASSERT(res.kind != TK_NONE);
  *op_type = min_type(*op_type, res);
  *lhs = min_type(*lhs, res);
  *rhs = min_type(*rhs, res);
}

/** Unify two types forwards through a multiplicative operator.
 *  Given the types of the operator arguments, this yields the operator type and
 * result type. The operator type and result type are almost always identical,
 * equal to the other type if either are real, else the most specific common
 * type of the two arguments. The exception here is when you are multiplying two
 * imaginary numbers, in which case the operation itself is performed in
 *  imaginary space (arbitrarily chosen, could just as easily be real) and the
 * result is real. op    res e.g. real * real   ==>  real  real cmplx * real ==>
 * cmplx cmplx imag * real   ==>  imag  imag imag * cmplx  ==>  cmplx cmplx imag
 * * imag   ==>  imag  real
 */
static void unify_mul_f(expr_type lhs, expr_type rhs, expr_type *op_type,
                        expr_type *res) {
  ASSERT(lhs.kind != TK_NONE);
  ASSERT(rhs.kind != TK_NONE);
  ASSERT(res);
  if (lhs.kind == TK_REAL) {
    *res = *op_type = rhs;
  } else if (rhs.kind == TK_REAL) {
    *res = *op_type = lhs;
  } else if (lhs.kind == TK_IMAG && rhs.kind == TK_IMAG) {
    *op_type = TK_IMAG;
    *res = TK_REAL;
  } else {
    *res = *op_type = max_type(lhs, rhs);
  }
}

/** Refine types backwards against a multiplicative operator, all types should
 * be set on entry. If the operator or result types are narrower than they would
 * otherwise be from the operator argument types provided, we can narrow these
 * operator argument types to avoid doing unnecessary work. e.g. real = real *
 * cmplx  ==>  real = real * real imag = imag * cmplx  ==>  imag = imag * real
 */
static void unify_mul_b(expr_type *lhs, expr_type *rhs, expr_type *op_type,
                        expr_type res) {
  ASSERT(lhs);
  ASSERT(rhs);
  ASSERT(lhs->kind != TK_NONE);
  ASSERT(rhs->kind != TK_NONE);
  ASSERT(res.kind != TK_NONE);
  if (res.kind == TK_COMPLEX ||
      ((lhs->kind == TK_COMPLEX) == (rhs->kind == TK_COMPLEX))) {
    return;
  }
  if (rhs->kind == TK_COMPLEX) {
    unify_mul_b(rhs, lhs, op_type, res);
    return;
  }
  switch (rhs->kind) {
  case TK_REAL:
    *op_type = min_type(*op_type, res);
    *lhs = min_type(*lhs, res);
    break;
  case TK_IMAG:
    *op_type = min_type(*op_type, conj_type(res.kind));
    *lhs = min_type(*lhs, conj_type(res.kind));
    break;
  case TK_NONE:
  case TK_COMPLEX:
    ASSERT(false);
  }
}

/// Unify two types forwards through the specified operator
static void unify_forwards(char op, expr_type lhs, expr_type rhs,
                           expr_type *op_type, expr_type *res) {
  switch (op) {
  case '+':
  case '-':
    unify_addsub_f(lhs, rhs, op_type, res);
    break;
  case '*':
    unify_mul_f(lhs, rhs, op_type, res);
    break;
  default:
    ASSERT(false);
  }
}

/// Refine types backwards against the specified operator.
static void unify_backwards(char op, expr_type *lhs, expr_type *rhs,
                            expr_type *op_type, expr_type res) {
  switch (op) {
  case '+':
  case '-':
    unify_addsub_b(lhs, rhs, op_type, res);
    break;
  case '*':
    unify_mul_b(lhs, rhs, op_type, res);
    break;
  default:
    ASSERT(false);
  }
}

std::map<atom, expr_type> type_check(std::list<expr_t> &algo,
                                     const io_ptr_t &iop, expr_type in_type,
                                     expr_type out_type) {
  // the declared type of each variable
  std::map<atom, expr_type> vars;

  // the most-specific use of each variable (e.g. only the real part of a
  // complex decl may be used) in which case we can use the unify-backwards
  // refinement to avoid doing unnecessary work.
  std::map<atom, expr_type> vars_max_use;

  auto get_type = [&](atom &a) -> expr_type {
    switch (a.kind) {
    case RT_NONE:
      ASSERT(false);
    case RT_REAL_CONST:
      return a.self_type = TK_REAL;
    case RT_IMAG_CONST:
      return a.self_type = TK_IMAG;
    case RT_WPTR:
      return a.self_type = TK_COMPLEX;
    case RT_PTR:
      if (is_in_ptr(iop, a)) {
        return vars[a] = a.self_type = in_type;
      } else if (is_out_ptr(iop, a)) {
        return vars[a] = a.self_type = out_type;
      } else {
        ASSERT(is_local_ptr(iop, a));
        return a.self_type = vars.at(a);
      }
    }
    ASSERT(false);
  };

  // part i) work forwards
  for (auto it = algo.begin(); it != algo.end(); ++it) {
    // figure out left type
    auto left_type = get_type(it->left);
    expr_type res_type;
    if (!it->right.kind) {
      res_type = it->op_type = left_type;
    } else {
      unify_forwards(it->op, left_type, get_type(it->right), &it->op_type,
                     &res_type);
    }
    if (is_out_ptr(iop, it->lhs)) {
      // TODO: assert res_type <= out_type
    } else {
      ASSERT(is_local_ptr(iop, it->lhs));
    }
    ASSERT(vars.find(it->lhs) == vars.end());
    vars[it->lhs] = it->lhs.self_type = res_type;
  }

  // part ii) work backwards
  for (auto it = algo.rbegin(); it != algo.rend(); ++it) {
    expr_type lhs_type = get_type(it->lhs);
    if (is_local_ptr(iop, it->lhs)) {
      vars[it->lhs] = lhs_type = it->lhs.self_type =
          min_type(lhs_type, vars_max_use.at(it->lhs));
    }
    if (!it->right.kind) {
      it->left.self_type = it->op_type = min_type(lhs_type, get_type(it->left));
    } else {
      unify_backwards(it->op, &it->left.self_type, &it->right.self_type,
                      &it->op_type, lhs_type);
    }
    for (auto *elem : {&it->left, &it->right}) {
      if (elem->kind == RT_PTR) {
        auto it2 = vars_max_use.find(*elem);
        if (it2 == vars_max_use.end()) {
          vars_max_use[*elem] = elem->self_type;
        } else {
          vars_max_use[*elem] = max_type(it2->second, elem->self_type);
        }
      }
    }
  }

  // part iii) work forwards, fixup self_type's
  // these may now be too specific thanks to the previous backwards pass,
  // if e.g. one use of a value only needed the real component of a value, but
  // another use required the entire complex def, we need to force all uses to
  // the latter!
  for (auto it = algo.begin(); it != algo.end(); ++it) {
    for (auto *elem : {&it->left, &it->right}) {
      auto it2 = vars.find(*elem);
      if (it2 != vars.end() && *elem == it2->first) {
        elem->self_type = it2->second;
      }
    }
  }
  return vars;
}

} // namespace plfft::wfta
