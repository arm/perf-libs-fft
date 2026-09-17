/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "simple_test.hpp"

#include <cinttypes>
#include <complex>

int main() {
  int ret = EXIT_SUCCESS;
  for (int64_t n = 2; n < 129; n++) {
    ret = simple_test<std::complex<__fp16>, std::complex<__fp16>>(n, 0.001);
    if (ret != EXIT_SUCCESS) {
      printf("Half-precision c2c test failure for n = %" PRId64 "\n", n);
      return ret;
    }
    ret = simple_test<__fp16, std::complex<__fp16>>(n, 0.001);
    if (ret != EXIT_SUCCESS) {
      printf("Half-precision r2c / c2r test failure for n = %" PRId64 "\n", n);
      return ret;
    }

    ret = simple_test<std::complex<float>, std::complex<float>>(n);
    if (ret != EXIT_SUCCESS) {
      printf("Single-precision c2c test failure for n = %" PRId64 "\n", n);
      return ret;
    }
    ret = simple_test<float, std::complex<float>>(n);
    if (ret != EXIT_SUCCESS) {
      printf("Single-precision r2c / c2r test failure for n = %" PRId64 "\n",
             n);
      return ret;
    }

    ret = simple_test<std::complex<double>, std::complex<double>>(n);
    if (ret != EXIT_SUCCESS) {
      printf("Double-precision c2c test failure for n = %" PRId64 "\n", n);
      return ret;
    }
    ret = simple_test<double, std::complex<double>>(n);
    if (ret != EXIT_SUCCESS) {
      printf("Double-precision r2c / c2r test failure for n = %" PRId64 "\n",
             n);
      return ret;
    }
  }
}
