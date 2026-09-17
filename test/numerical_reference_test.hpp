/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "known_pairs_test.hpp"

#include <complex>
#include <cstdlib>
#include <vector>

template<typename T>
auto generate_random_input() {
  using real_t = remove_complex_t<T>;
  auto random_value = []() {
    return static_cast<real_t>(static_cast<double>(rand()) / RAND_MAX);
  };
  return [random_value](T *x, const IOLayout &c) {
    for (int i = 0; i < c.howmany; i++) {
      for (int j = 0; j < c.n; j++) {
        if constexpr (std::is_same_v<real_t, T>) {
          x[i * c.dist + j * c.stride] = random_value();
        } else {
          x[i * c.dist + j * c.stride] = T{random_value(), random_value()};
        }
      }
    }
  };
}

template<typename T>
auto calculate_reference() {
  return [](const T *input, T *out, const TestCase &c,
            const plfft_direction_t dir) {
    const auto isize = calculate_size(c.n, c.howmany, c.istride, c.idist);
    std::vector<std::complex<double>> ref_in(isize);
    for (int i = 0; i < c.howmany; i++) {
      for (int j = 0; j < c.n; j++) {
        const auto input_idx = i * c.idist + j * c.istride;
        ref_in[input_idx] = {input[input_idx].real(), input[input_idx].imag()};
      }
    }

    const auto osize = calculate_size(c.n, c.howmany, c.ostride, c.odist);
    std::vector<std::complex<double>> ref_out(osize);
    auto plan =
        plfft::make_batched_1d_plan<std::complex<double>, std::complex<double>>(
            c.n, c.howmany, c.istride, c.idist, c.ostride, c.odist, dir,
            PLFFT_IO_NO_ALIAS);
    plan->execute(ref_in.data(), ref_out.data());

    for (int i = 0; i < c.howmany; i++) {
      for (int j = 0; j < c.n; j++) {
        const auto output_idx = i * c.odist + j * c.ostride;
        out[output_idx] = ref_out[output_idx];
      }
    }
  };
}

template<typename T>
int numerical_reference_test(const TestCase &c, const plfft_direction_t dir,
                             Planner<T, T> *make_plan,
                             const remove_complex_t<T> calculated_tolerance) {
  return plfft_numerical_reference_test<T>(
      // Initialize input vector: random input
      generate_random_input<T>(),
      // Calculate output using the double-precision non-SME implementation.
      calculate_reference<T>(), c, dir, make_plan, calculated_tolerance);
}
