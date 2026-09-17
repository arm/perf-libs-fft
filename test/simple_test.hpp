/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "arm_fft1d.hpp"
#include "test_utils.hpp"

#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <vector>

template<typename Tx, typename Ty>
int simple_test(
    const int64_t n, double eps = 0.0,
    Planner<Tx, Ty> *make_plan_fwd = &plfft::make_batched_1d_plan<Tx, Ty>,
    Planner<Tx, Ty> *make_plan_bck = &plfft::make_batched_1d_plan<Ty, Tx>) {
  using real_t = remove_complex_t<Tx>;

  const int64_t howmany = 1;
  const int64_t istride = 1;
  const int64_t idist = 1;
  const int64_t ostride = 1;
  const int64_t odist = 1;

  real_t nf = n;
  eps = eps == 0.0 ? (double)std::numeric_limits<real_t>::epsilon() : eps;
  double tol = 10 * 2 * eps * 4 * nf * std::log2(nf);
  double diff;

  std::cout << "Test n = " << n << std::endl;

  std::vector<Tx> in(n);
  std::vector<Ty> out(n);
  std::vector<Tx> out2(n);

  real_t sum_fwd = 0.;
  for (size_t i = 0; i < in.size(); i++) {
    real_t x = (i + 1) / nf;
    in[i] = x;
    sum_fwd += x;
  }

  auto plan_f = make_plan_fwd(n, howmany, istride, idist, ostride, odist,
                              PLFFT_FORWARD, PLFFT_IO_NO_ALIAS);
  plan_f->execute(in.data(), out.data());

  // For a quick test that the forward transform did
  // _something_ we check that the first element
  // contains the sum of the input
  diff = std::abs(out[0] - sum_fwd);
  diff = sum_fwd > tol ? diff / sum_fwd : diff;
  // Invert check to correctly detect NaNs.
  if (!(diff <= tol)) {
    return EXIT_FAILURE;
  }

  auto plan_b = make_plan_bck(n, howmany, istride, idist, ostride, odist,
                              PLFFT_BACKWARD, PLFFT_IO_NO_ALIAS);
  plan_b->execute(out.data(), out2.data());

  // Check first element of backwards transform is the sum of the input
  // (summation is more involved than for the forwards transform due to
  // Hermitian input)
  real_t sum_bck = real(out[0]);
  for (size_t i = 1; i < out.size() / 2; i++) {
    sum_bck += real(out[i]) * 2;
  }
  sum_bck += real(out[n / 2]) * (n % 2 == 0 ? 1 : 2);

  diff = std::abs(out2[0] - sum_bck);
  diff = sum_bck > tol ? diff / sum_bck : diff;
  // Invert check to correctly detect NaNs.
  if (!(diff <= tol)) {
    return EXIT_FAILURE;
  }

  // Check forwards-backwards round-trip
  for (size_t i = 0; i < out2.size(); i++) {
    auto res = real(out2[i]) / nf;
    auto ref = real(in[i]);
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

template<typename Tx, typename Ty>
int simple_test_sme(const int64_t n, double eps = 0.0) {
  return simple_test<Tx, Ty>(n, eps, &plfft::make_batched_1d_plan_sme<Tx, Ty>,
                             &plfft::make_batched_1d_plan_sme<Ty, Tx>);
}
