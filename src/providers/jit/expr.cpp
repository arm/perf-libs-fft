/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "expr.hpp"
#include "plfft_assert.hpp"

namespace plfft::wfta {

void expr::debug_print() const {
  fprintf(stderr, "lhs:%zu", lhs.ival);
  if (lhs.self_type.kind == TK_REAL) {
    fprintf(stderr, "(real) = ");
  } else if (lhs.self_type.kind == TK_IMAG) {
    fprintf(stderr, "(imag) = ");
  } else if (lhs.self_type.kind == TK_COMPLEX) {
    fprintf(stderr, "(complex) = ");
  }

  fprintf(stderr, "left:%zu", left.ival);
  if (left.self_type.kind == TK_REAL) {
    fprintf(stderr, "(real) ");
  } else if (left.self_type.kind == TK_IMAG) {
    fprintf(stderr, "(imag) ");
  } else if (left.self_type.kind == TK_COMPLEX) {
    fprintf(stderr, "(complex) ");
  }

  if (op) {
    fprintf(stderr, "%c ", op);
    if (right.kind == RT_REAL_CONST) {
      fprintf(stderr, "right:%f", right.rval);
    } else if (right.kind == RT_IMAG_CONST) {
      fprintf(stderr, "right:%f*i", right.rval);
    } else {
      fprintf(stderr, "right:%zu", right.ival);
    }
    if (right.self_type.kind == TK_REAL) {
      fprintf(stderr, "(real) ");
    } else if (right.self_type.kind == TK_IMAG) {
      fprintf(stderr, "(imag) ");
    } else if (right.self_type.kind == TK_COMPLEX) {
      fprintf(stderr, "(complex) ");
    }
  }

  if (op_type.kind == TK_REAL) {
    fprintf(stderr, "op:real ");
  } else if (op_type.kind == TK_IMAG) {
    fprintf(stderr, "op:imag ");
  } else if (op_type.kind == TK_COMPLEX) {
    fprintf(stderr, "op:complex ");
  }

  fprintf(stderr, "\n");
}

algo_flops expr::flops() const {
  if (!op) {
    return {0, 0, 0};
  }
  char identity_elem = op == '*' ? 1.0 : 0.0;
  if ((right.kind == RT_REAL_CONST || right.kind == RT_IMAG_CONST) &&
      std::abs(right.rval - identity_elem) <= RVAL_APPROX_MARGIN) {
    return {0, 0, 0};
  }
  // if both operands are complex then this is definitely a complex operation.
  // we don't try to consider whether we can fuse things into fmas later etc,
  // but we know that full complex multiplication is definitely going to use an
  // fma.
  if (op_type.kind == TK_COMPLEX && left.self_type.kind == TK_COMPLEX &&
      right.self_type.kind == TK_COMPLEX) {
    switch (op) {
    case '+':
      return {2, 0, 0};
    case '-':
      return {2, 0, 0};
    case '*':
      return {0, 2, 2};
    default:
      ASSERT(false && "unknown op?");
    }
  }
  // if only one operand is complex then even if the operation is complex then
  // we can execute this much more simply (albeit multiplies still need to
  // broadcast the scalar across both lanes, so 2 flops rather than 1).
  if (op_type.kind == TK_COMPLEX) {
    switch (op) {
    case '+':
      return {1, 0, 0};
    case '-':
      return {1, 0, 0};
    case '*':
      return {0, 2, 0};
    default:
      ASSERT(false && "unknown op?");
    }
  }
  // easy scalar flop, as expected.
  switch (op) {
  case '+':
    return {1, 0, 0};
  case '-':
    return {1, 0, 0};
  case '*':
    return {0, 1, 0};
  default:
    ASSERT(false && "unknown op?");
  }
}

} // end namespace plfft::wfta
