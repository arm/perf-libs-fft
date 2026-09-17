/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "advanced_test_utils.hpp"
#include "arm_fft1d.hpp"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int forward_sign = -1;
constexpr int backward_sign = 1;
constexpr double target_secs_total = 0.0;
constexpr double margin = 0.5;

size_t logical_real_offset(const TestCase &c, int64_t batch, int64_t index) {
  return batch * c.idist + index * c.istride;
}

double input_value(const TestCase &c, int64_t batch, int64_t index) {
  return ((batch + 1) * 100 + index + 1) / c.n;
}

bool close_enough(double result, double reference) {
  constexpr double tolerance = 1.0e-11;
  const double diff = std::abs(result - reference);
  const double scale = std::max(1.0, std::abs(reference));
  return diff <= tolerance * scale;
}

int r2c_c2r_test(const TestCase &c) {
  using complex_t = std::complex<double>;

  const size_t real_size =
      (c.howmany - 1) * c.idist + (c.n - 1) * c.istride + 1;
  const size_t complex_size =
      (c.howmany - 1) * c.odist + (c.n / 2) * c.ostride + 1;

  std::vector<double> in_orig(real_size, -999.0);
  std::vector<complex_t> out(complex_size, complex_t{-999.0, -999.0});

  for (int64_t batch = 0; batch < c.howmany; ++batch) {
    for (int64_t index = 0; index < c.n; ++index) {
      const size_t offset = logical_real_offset(c, batch, index);
      in_orig[offset] = input_value(c, batch, index);
    }
  }

  auto in = in_orig;
  auto in2 = in_orig;

  auto plan_f = plfft::make_batched_1d_plan<double, complex_t>(
      c.n, in.data(), out.data(), c.howmany, c.istride, c.idist, c.ostride,
      c.odist, forward_sign, static_cast<plfft_r2r_kind_t>(0),
      target_secs_total, margin);
  auto plan_b = plfft::make_batched_1d_plan<complex_t, double>(
      c.n, out.data(), in2.data(), c.howmany, c.ostride, c.odist, c.istride,
      c.idist, backward_sign, static_cast<plfft_r2r_kind_t>(0),
      target_secs_total, margin);

  if (!plan_f || !plan_b) {
    printf("Failed to create PLFFT plans\n");
    return EXIT_FAILURE;
  }

  plan_f->execute(in.data(), out.data());
  plan_b->execute(out.data(), in2.data());

  bool passed = true;
  for (int64_t batch = 0; batch < c.howmany; ++batch) {
    for (int64_t index = 0; index < c.n; ++index) {
      const size_t offset = logical_real_offset(c, batch, index);
      const double result = in2[offset] / c.n;
      const double reference = in_orig[offset];
      if (in[offset] != in_orig[offset]) {
        printf("Input value modified: batch=%" PRId64 ", index=%" PRId64
               ", offset=%zu, in=%.17g, in_orig=%.17g\n",
               batch, index, offset, in[offset], in_orig[offset]);
        passed = false;
      }
      if (!close_enough(result, reference)) {
        printf("Mismatch: batch=%" PRId64 ", index=%" PRId64
               ", offset=%zu, result=%.17g, reference=%.17g, "
               "abs_error=%.17g\n",
               batch, index, offset, result, reference,
               std::abs(result - reference));
        passed = false;
      }
    }
  }

  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

int main() {
  const std::vector<TestCase> test_cases_r2c_c2r = {
      // clang-format off
      //    n  howmany  istride  idist  ostride  odist
      {    22,       3,       3,    88,       3,    90 },
      {    46,       4,       2,   100,       2,    92 },
      {    12,       5,       1,    12,       1,    12 },
      {   128,       6,       5,   650,       2,   256 },
      {    77,       7,       4,   308,       3,   231 },
      // clang-format on
  };

  for (const auto &tc : test_cases_r2c_c2r) {
    printf("Testing n = %" PRId64 ", howmany = %" PRId64 "\n", tc.n,
           tc.howmany);

    const auto ret = r2c_c2r_test(tc);
    if (ret != EXIT_SUCCESS) {
      printf("Double-precision r2c/c2r failed for n = %" PRId64 "\n", tc.n);
      return ret;
    }
  }

  return EXIT_SUCCESS;
}
