/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "known_pairs_test.hpp"

template<typename T>
auto initialise_constant_signal(double amp) {
  using real_t = remove_complex_t<T>;
  const real_t val = convert_to<real_t>{}(amp);
  return [val](T *x, const IOLayout &c) {
    for (int i = 0; i < c.howmany; i++) {
      for (int j = 0; j < c.n; j++) {
        x[i * c.dist + j * c.stride] = val;
      }
    }
  };
}

template<typename T>
auto initialise_delta_with_amp(double amp) {
  using real_t = remove_complex_t<T>;
  const real_t val = convert_to<real_t>{}(amp);
  return [val](T *x, const IOLayout &c) {
    for (int i = 0; i < c.howmany; i++) {
      x[i * c.dist] = val;
      for (int j = 1; j < c.n; j++) {
        x[i * c.dist + j * c.stride] = 0;
      }
    }
  };
}

template<typename T>
int constant_to_delta_test(
    const TestCase &c,
    Planner<T, T> *make_plan = &plfft::make_batched_1d_plan<T, T>) {
  const double scale = std::is_integral_v<remove_complex_t<T>> ? c.n : 1;

  return known_pairs_test<T>(
      // Initialize input vector: all elements are 1
      initialise_constant_signal<T>(1),
      // Initialize reference vector: an impulse at index 0, with amplitude n
      // (or amplitude 1 for fixed-point transforms, due to scaling)
      initialise_delta_with_amp<T>(c.n / scale), c, make_plan);
}

template<typename T>
int delta_to_constant_test(
    const TestCase &c,
    Planner<T, T> *make_plan = &plfft::make_batched_1d_plan<T, T>) {
  const double scale = std::is_integral_v<remove_complex_t<T>> ? c.n : 1;
  return known_pairs_test<T>(
      // Initialize input vector: an impulse at index 0, with amplitude 1
      initialise_delta_with_amp<T>(1),
      // Initialize reference vector: all elements are 1
      // (or 1/n for fixed-point transforms, due to scaling)
      initialise_constant_signal<T>(1 / scale), c, make_plan);
}
