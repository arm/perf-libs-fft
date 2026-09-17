/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "arm_fft1d.hpp"
#include <cinttypes>
#include <cmath>
#include <complex>
#include <limits>

extern "C" int c_test(const int64_t n, double eps) {

  const int64_t howmany = 1;
  const int64_t istride = 1;
  const int64_t idist = 1;
  const int64_t ostride = 1;
  const int64_t odist = 1;
  const int sign_f = -1;
  const int sign_b = 1;
  const plfft_r2r_kind_t kind = static_cast<plfft_r2r_kind_t>(0);
  const double target_secs_total = 0.0;
  const double margin = 0.0;

  printf("Test n = %" PRId64 "\n", n);

  std::complex<double> *in =
      (std::complex<double> *)malloc(n * sizeof(std::complex<double>));
  std::complex<double> *out =
      (std::complex<double> *)malloc(n * sizeof(std::complex<double>));
  std::complex<double> *out2 =
      (std::complex<double> *)malloc(n * sizeof(std::complex<double>));

  for (int64_t i = 0; i < n; i++) {
    in[i] = i;
  }

  auto plan_f = plfft::make_batched_1d_plan(n, in, out, howmany, istride, idist,
                                            ostride, odist, sign_f, kind,
                                            target_secs_total, margin);

  auto plan_b = plfft::make_batched_1d_plan(n, out, out2, howmany, istride,
                                            idist, ostride, odist, sign_b, kind,
                                            target_secs_total, margin);

  if (!plan_f) {
    printf("Failed to create forward plan for n = %" PRId64 "\n", n);
    free(in);
    free(out);
    free(out2);
    return EXIT_FAILURE;
  }
  if (!plan_b) {
    printf("Failed to create backward plan for n = %" PRId64 "\n", n);
    free(in);
    free(out);
    free(out2);
    return EXIT_FAILURE;
  }

  plan_f->execute(in, out);

  plan_b->execute(out, out2);

  double nf = n;
  eps = eps == 0.0 ? (double)std::numeric_limits<double>::epsilon() : eps;
  double tol = 10 * 2 * eps * 4 * nf * std::log2(nf);
  for (int64_t i = 0; i < n; i++) {
    auto res = out2[i].real() / nf;
    auto ref = in[i].real();
    double diff = std::abs(res - ref);
    diff = ref > tol ? diff / ref : diff;
    printf("ref = %le res = %le\n", ref, res);
    if (diff > tol) {
      return EXIT_FAILURE;
    }
  }

  free(in);
  free(out);
  free(out2);

  printf(" ---- Success!\n");
  return EXIT_SUCCESS;
}
