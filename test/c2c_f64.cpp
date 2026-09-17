/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "advanced_test_utils.hpp"

int main() {
  const std::vector<TestCase> test_cases_c2c = {
      // clang-format off
      //    n  howmany  istride   idist  ostride   odist
      {     2,       1,       1,       2,       1,      2 },
      {     4,       1,       1,       4,       1,      4 },
      {     8,       1,       1,       8,       1,      8 },
      {    16,       4,       1,      16,       1,     16 },
      {     8,       3,       2,      20,       2,     20 },
      {    64,       2,       8,     600,       8,    600 },
      {    23,      49,     441,       9,     441,      9 },
      {    24,       4,       1,      32,       1,     32 },
      {   512,       1,       1,     512,       1,    512 },
      {  1024,       4,       1,    1024,       1,   1024 },
      {  1024,       4,    1024,       1,       1,   1024 },
      {  1058,       4,    1058,       1,    1058,      1 },
      // clang-format on
  };

  // run c2c tests
  for (const auto &tc : test_cases_c2c) {
    printf("Testing n = %" PRId64 ", howmany = %" PRId64 "\n", tc.n,
           tc.howmany);

    auto ret =
        advanced_interface_test<std::complex<double>, std::complex<double>>(tc);
    if (ret != EXIT_SUCCESS) {
      printf("Double-precision c2c failed for n = %" PRId64 "\n", tc.n);
      return ret;
    }
  }
}
