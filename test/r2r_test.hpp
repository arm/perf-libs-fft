/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "arm_fft1d.hpp"
#include "test_utils.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

inline const char *r2r_name(plfft_r2r_kind_t kind) {
  switch (kind) {
    // clang-format off
  case PLFFT_R2R_DCT_1: return "DCT_1";
  case PLFFT_R2R_DCT_2: return "DCT_2";
  case PLFFT_R2R_DCT_3: return "DCT_3";
  case PLFFT_R2R_DCT_4: return "DCT_4";
  case PLFFT_R2R_DST_1: return "DST_1";
  case PLFFT_R2R_DST_2: return "DST_2";
  case PLFFT_R2R_DST_3: return "DST_3";
  case PLFFT_R2R_DST_4: return "DST_4";
  case PLFFT_R2R_DHT  : return "DHT";
  case PLFFT_R2R_R2HC : return "R2HC";
  case PLFFT_R2R_HC2R : return "HC2R";
  default: __builtin_unreachable();
    // clang-format on
  }
}

inline plfft_r2r_kind_t r2r_inverse(plfft_r2r_kind_t kind) {
  switch (kind) {
    // clang-format off
  case PLFFT_R2R_DCT_1:
  case PLFFT_R2R_DCT_4:
  case PLFFT_R2R_DST_1:
  case PLFFT_R2R_DST_4:
  case PLFFT_R2R_DHT:
    return kind; // self-inverse

  case PLFFT_R2R_DCT_2: return PLFFT_R2R_DCT_3;
  case PLFFT_R2R_DCT_3: return PLFFT_R2R_DCT_2;

  case PLFFT_R2R_DST_2: return PLFFT_R2R_DST_3;
  case PLFFT_R2R_DST_3: return PLFFT_R2R_DST_2;

  case PLFFT_R2R_R2HC: return PLFFT_R2R_HC2R;
  case PLFFT_R2R_HC2R: return PLFFT_R2R_R2HC;

  default: __builtin_unreachable();
    // clang-format on
  }
}

inline long double r2r_scale(int64_t n, plfft_r2r_kind_t kind) {
  switch (kind) {
  case PLFFT_R2R_DCT_1:
    return 2 * (n - 1);
  case PLFFT_R2R_DST_1:
    return 2 * (n + 1);
  case PLFFT_R2R_DHT:
  case PLFFT_R2R_R2HC:
  case PLFFT_R2R_HC2R:
    return n;
  case PLFFT_R2R_DCT_2:
  case PLFFT_R2R_DCT_3:
  case PLFFT_R2R_DCT_4:
  case PLFFT_R2R_DST_2:
  case PLFFT_R2R_DST_3:
  case PLFFT_R2R_DST_4:
    return 2 * n;
  default:
    __builtin_unreachable();
  }
}

template<typename T>
int r2r_test(const int64_t n, plfft_r2r_kind_t kind_f, double eps = 0.0) {
  using real_t = remove_complex_t<T>;

  const int64_t howmany = 1;
  const int64_t istride = 1;
  const int64_t idist = 1;
  const int64_t ostride = 1;
  const int64_t odist = 1;

  real_t nf = n;
  eps = eps == 0.0 ? (double)std::numeric_limits<real_t>::epsilon() : eps;
  double tol = 10 * 2 * eps * 4 * nf * std::log2(nf);
  double diff;

  std::cout << "Test n = " << n << ", kind = " << r2r_name(kind_f) << std::endl;

  std::vector<T> in(n);
  std::vector<T> out(n);
  std::vector<T> out2(n);

  real_t sum_fwd = 0.;
  for (size_t i = 0; i < in.size(); i++) {
    real_t x = (i + 1) / nf;
    in[i] = x;
    sum_fwd += x;
  }

  auto plan_f = plfft::make_batched_1d_r2r_plan<T>(
      n, howmany, istride, idist, ostride, odist, kind_f, PLFFT_IO_NO_ALIAS);
  if (!plan_f) {
    return EXIT_FAILURE;
  }

  plan_f->execute(in.data(), out.data());

  // For a quick test that the forward transform did _something_ we check the
  // first element (but only when there is a simple formula).
  const auto first_element_is_sum_f = kind_f == PLFFT_R2R_DCT_2 ||
                                      kind_f == PLFFT_R2R_DHT ||
                                      kind_f == PLFFT_R2R_R2HC;
  if (first_element_is_sum_f) {
    const auto scale = kind_f == PLFFT_R2R_DCT_2 ? 2 : 1;
    const auto ref = scale * sum_fwd;
    diff = std::abs((long double)out[0] - ref);
    diff = sum_fwd > tol ? diff / sum_fwd : diff;
    // Invert check to correctly detect NaNs.
    if (!(diff <= tol)) {
      return EXIT_FAILURE;
    }
  }

  const auto kind_b = r2r_inverse(kind_f);
  auto plan_b = plfft::make_batched_1d_r2r_plan<T>(
      n, howmany, istride, idist, ostride, odist, kind_b, PLFFT_IO_NO_ALIAS);
  if (!plan_b) {
    return EXIT_FAILURE;
  }

  plan_b->execute(out.data(), out2.data());

  // Check first element of backwards transform is the scaled sum of the input
  // (but only when there is a simple formula).
  const auto first_element_is_sum_b = kind_b == PLFFT_R2R_DCT_2 ||
                                      kind_b == PLFFT_R2R_DHT ||
                                      kind_b == PLFFT_R2R_R2HC;
  if (first_element_is_sum_b) {
    long double sum_bck = 0.;
    for (size_t i = 0; i < out.size(); i++) {
      sum_bck += real(out[i]);
    }
    const auto scale = kind_b == PLFFT_R2R_DCT_2 ? 2 : 1;
    const auto ref = scale * sum_bck;
    diff = std::abs((long double)out2[0] - ref);
    diff = ref > tol ? diff / ref : diff;
    // Invert check to correctly detect NaNs.
    if (!(diff <= tol)) {
      return EXIT_FAILURE;
    }
  }

  // Check forwards-backwards round-trip
  const auto scale = r2r_scale(n, kind_f);
  for (size_t i = 0; i < out2.size(); i++) {
    auto res = (long double)real(out2[i]) / scale;
    auto ref = (long double)real(in[i]);
    diff = std::abs(res - ref);
    diff = ref > tol ? diff / ref : diff;
    // Invert check to correctly detect NaNs.
    if (!(diff <= tol)) {
      return EXIT_FAILURE;
    }
  }

  std::cout << " ---- Success!" << std::endl;
  return EXIT_SUCCESS;
}
