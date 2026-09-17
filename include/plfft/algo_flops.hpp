/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace plfft {

/**
 * Keeps track of the number of floating-point operations associated with the
 * algorithm that a particular kernel is based on. This may not correspond to
 * the actual code produced since the algorithm may include redundant
 * operations or other room for simplifications.
 */
struct algo_flops {
  int add = 0;
  int mul = 0;
  int fma = 0;

  algo_flops operator+(algo_flops other) const {
    return {add + other.add, mul + other.mul, fma + other.fma};
  }

  algo_flops operator*(int s) const {
    return {add * s, mul * s, fma * s};
  }

  algo_flops &operator+=(algo_flops other) {
    add += other.add;
    mul += other.mul;
    fma += other.fma;
    return *this;
  }
};

} // namespace plfft
