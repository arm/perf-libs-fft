/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "advanced_test_utils.hpp"

#include <complex>
#include <vector>

namespace {

int run_test(const TestCase &c) {
  const size_t input_size =
      (c.howmany - 1) * c.idist + (c.n - 1) * c.istride + 1;
  const size_t output_size =
      (c.howmany - 1) * c.odist + (c.n / 2) * c.ostride + 1;

  std::vector<float> input(input_size);
  std::vector<double> reference_input(input_size);
  std::vector<std::complex<float>> output(output_size);
  std::vector<std::complex<double>> reference(output_size);

  for (int64_t batch = 0; batch < c.howmany; ++batch) {
    for (int64_t i = 0; i < c.n; ++i) {
      const size_t index = batch * c.idist + i * c.istride;
      input[index] = static_cast<float>((batch + i + 1.0) / c.n);
      reference_input[index] = input[index];
    }
  }

  auto plan = plfft::make_batched_1d_plan_sme<float, std::complex<float>>(
      c.n, c.howmany, c.istride, c.idist, c.ostride, c.odist, PLFFT_FORWARD,
      PLFFT_IO_NO_ALIAS);

  auto reference_plan =
      plfft::make_batched_1d_plan<double, std::complex<double>>(
          c.n, c.howmany, c.istride, c.idist, c.ostride, c.odist, PLFFT_FORWARD,
          PLFFT_IO_NO_ALIAS);
  plan->execute(input.data(), output.data());
  reference_plan->execute(reference_input.data(), reference.data());

  const double tolerance = default_tol<float>(c.n);
  for (int64_t batch = 0; batch < c.howmany; ++batch) {
    for (int64_t i = 0; i <= c.n / 2; ++i) {
      const size_t index = batch * c.odist + i * c.ostride;
      const std::complex<double> result = output[index];
      const auto [success, error] = check(result, reference[index], tolerance);
      if (!success) {
        print_failure_header(c, tolerance);
        print_failure(batch, i, result, reference[index], error);
        return EXIT_FAILURE;
      }
    }
  }
  return EXIT_SUCCESS;
}

} // namespace

int main() {
  const std::vector<TestCase> cases = {
      // clang-format off
      //   n  howmany  istride  idist  ostride  odist
      {  256,      256,       1,   256,       1,   129 },
      {  256,        4,       1,   259,       1,   132 },
      // clang-format on
  };

  for (const auto &c : cases) {
    if (run_test(c) != EXIT_SUCCESS) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
