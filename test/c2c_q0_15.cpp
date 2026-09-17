/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "advanced_test_utils.hpp"

#include <complex>
#include <cstdint>
#include <vector>

int main() {
  const std::vector<TestCase> test_cases_c2c = {
      // clang-format off
      //    n  howmany  istride   idist  ostride   odist
      {     2,       1,       1,       0,       1,      0 },
      {     4,       1,       1,       0,       1,      0 },
      {     8,       1,       1,       0,       1,      0 },
      {    16,       1,       1,      16,       1,     16 },
      {     8,       1,       2,      20,       2,     20 },
      {    64,       2,       8,     600,       8,    600 },
      {    32,       4,       1,      32,       1,     32 },
      {   512,       1,       1,     512,       1,    512 },
      {  1024,       4,       1,    1024,       1,   1024 },
      {  1024,       4,       1,    1024,    1024,      1 },
      // clang-format on
  };

  for (const auto &tc : test_cases_c2c) {
    printf("Testing n = %" PRId64 ", howmany = %" PRId64 "\n", tc.n,
           tc.howmany);

    const auto ret =
        advanced_interface_test<std::complex<int16_t>, std::complex<int16_t>>(
            tc);
    if (ret != EXIT_SUCCESS) {
      printf("Q0.15 c2c failed for n = %" PRId64 "\n", tc.n);
      return ret;
    }
  }
}
