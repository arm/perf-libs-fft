/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "advanced_test_utils.hpp"

#include "arm_fft1d.hpp"

#include <optional>
#include <vector>

inline int64_t calculate_size(const int64_t n, const int64_t howmany,
                              const int64_t stride, const int64_t dist) {
  return std::max(howmany, stride) * std::max(n, dist);
}

template<typename T, typename TestCaseTempl>
int calculate_solution(const T *input, T *output, Planner<T, T> *make_plan,
                       const TestCaseTempl &c, const plfft_direction_t dir) {
  auto plan = make_plan(c.n, c.howmany, c.istride, c.idist, c.ostride, c.odist,
                        dir, PLFFT_IO_NO_ALIAS);
  if (!plan) {
    return 1;
  }

  plan->execute(input, output);

  return 0;
}

template<typename T, typename TestCaseTempl>
int validate_solution(const T *out, const T *ref, const TestCaseTempl &c,
                      const remove_complex_t<T> tol) {
  int status = 0;
  for (int k = 0; k < c.howmany; k++) {
    for (int i = 0; i < c.n; i++) {
      const T out_elem = out[k * c.odist + i * c.ostride];
      const T ref_elem = ref[k * c.odist + i * c.ostride];
      const auto [success, err] = check(out_elem, ref_elem, tol);

      if (!success) {
        if (status == 0) {
          print_failure_header(c, tol);
        }
        status = 1;
        print_failure(k, i, out_elem, ref_elem, err);
      }
    }
  }
  return status;
}

template<typename T, typename PrepareIn, typename PrepareRef,
         typename TestCaseTempl>
int known_pairs_test(PrepareIn prepare_in, PrepareRef prepare_ref,
                     const TestCaseTempl &c, Planner<T, T> *make_plan,
                     const std::optional<double> tolerance = std::nullopt) {
  /* Generic helper for 'known pairs' tests, in which the input to the FFT has
     some special form such that the output can be computed analytically. Pass
     callables to set up the input and a reference to the output for
     verification. By construction it only calculates and checks a forward FFT.
   */
  using real_t = remove_complex_t<T>;

  const auto isize = calculate_size(c.n, c.howmany, c.istride, c.idist);
  std::vector<T> in(isize);
  prepare_in(in.data(), c.ilayout());

  const auto osize = calculate_size(c.n, c.howmany, c.ostride, c.odist);
  std::vector<T> out(osize), ref(osize);
  prepare_ref(ref.data(), c.olayout());

  int status =
      calculate_solution<T>(in.data(), out.data(), make_plan, c, PLFFT_FORWARD);
  if (status != 0) {
    return status;
  }

  const real_t tol = tolerance.value_or(default_tol<real_t>(c.n));
  status = validate_solution<T>(out.data(), ref.data(), c, tol);

  return status;
}

template<typename T, typename PrepareIn, typename PrepareRef,
         typename TestCaseTempl>
int plfft_numerical_reference_test(
    PrepareIn prepare_in, PrepareRef prepare_ref, const TestCaseTempl &c,
    const plfft_direction_t dir, Planner<T, T> *make_plan,
    const remove_complex_t<T> calculated_tolerance) {
  /* Generic helper for unit test where we want to compare the FFT calculated
     with some new/dedicated techniques and compare it with the reference
     PLFFT implementation not using those techniques. */

  const auto isize = calculate_size(c.n, c.howmany, c.istride, c.idist);
  std::vector<T> in(isize);
  prepare_in(in.data(), c.ilayout());

  const auto osize = calculate_size(c.n, c.howmany, c.ostride, c.odist);
  std::vector<T> out(osize), ref(osize);
  prepare_ref(in.data(), ref.data(), c, dir);

  int status = calculate_solution<T>(in.data(), out.data(), make_plan, c, dir);
  if (status != 0) {
    return status;
  }

  status =
      validate_solution<T>(out.data(), ref.data(), c, calculated_tolerance);

  return status;
}
