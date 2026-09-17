/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "known_pairs_test.hpp"

#include <optional>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

struct PureToneTestCase : public TestCase {
  const int k0;
  const std::optional<double> tolerance = std::nullopt;
};

template<typename T>
auto initialise_impulse(const int64_t k0) {
  /* A spike with amplitude 1 at k0.  */
  using real_t = remove_complex_t<T>;
  return [k0](T *x, const IOLayout &c) {
    for (int k = 0; k < c.howmany; k++) {
      for (int i = 0; i < c.n; i++) {
        const double v = i == k0 ? 1.0 : 0.0;
        x[k * c.dist + i * c.stride] = convert_to<real_t>{}(v);
      }
    }
  };
}

template<typename T>
auto initialise_pure_tone(const int64_t k0, const double amplitude) {
  /* X[k] = A * e^(-2 * pi * i * k * k0 / n).  */
  using real_t = remove_complex_t<T>;
  return [k0, amplitude](T *x, const IOLayout &c) {
    const double theta = (2 * M_PI * k0) / c.n;
    const std::complex<double> w(std::cos(theta), -std::sin(theta));
    std::complex<double> u = amplitude;
    for (int i = 0; i < c.n; i++) {
      for (int k = 0; k < c.howmany; k++) {
        x[k * c.dist + i * c.stride] = {convert_to<real_t>{}(u.real()),
                                        convert_to<real_t>{}(u.imag())};
      }
      u *= w;
    }
  };
}

template<typename T>
int pure_tone_fwd_test(
    const PureToneTestCase &c,
    Planner<T, T> *make_plan = &plfft::make_batched_1d_plan<T, T>) {
  const double scale = std::is_integral_v<remove_complex_t<T>> ? c.n : 1;
  return known_pairs_test<T>(initialise_impulse<T>(c.k0),
                             initialise_pure_tone<T>(c.k0, 1 / scale), c,
                             make_plan, c.tolerance);
}
