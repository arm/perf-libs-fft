/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "runtime_precision.hpp"

namespace plfft {

struct twiddle_layout {
  /** What precision is the kernel expecting twiddle factors in?
   *  (This may be different if e.g. the machine does not support
   *   half precision at runtime) */
  runtime_precision precision;

  /** Do we want pre-multiplied twiddle factors (i.e. (-imag, real)).
   *  This is useful if we cannot natively do complex multiplication
   *  such as when we lack the FCMLA instruction. */
  bool want_premul;

  /** How many twiddle factors to interleave in the main body of the loop. This
   * is usually identical to the unroll factor of the kernel loop. */
  int interleave_factor;

  /** Whether to include the unity twiddle row (j = 0) in the twiddle table. */
  bool include_unity_row = false;
};

} // namespace plfft
