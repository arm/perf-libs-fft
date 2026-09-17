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
      {     8,       1,       1,       8,       1,      8 },
      {    16,       4,       1,      16,       1,     16 },
      {     8,       3,       2,      20,       2,     20 },
      {    64,       2,       8,     600,       8,    600 },
      {    24,       4,       1,      32,       1,     32 },
      {   256,     256,       1,     256,       1,    256 },
      {   256,      29,       1,     256,       1,    256 },
      {   256,     256,     256,       1,     256,      1 },
      {   256,      29,      29,       1,      29,      1 },
      {   512,       1,       1,     512,       1,    512 },
      {  1024,       4,       1,    1024,       1,   1024 },
      {  1024,       4,       1,    1024,    1024,      1 },
      {  1058,       4,    1058,       1,    1058,      1 },
      {     2,      11,       1,       2,      11,      1 }, //tu
      {     3,      11,       1,       3,      11,      1 }, //tu
      {     4,      11,       1,       4,      11,      1 }, //tu
      {     5,      11,       1,       5,      11,      1 }, //tu
      {     6,      11,       1,       6,      11,      1 }, //tu
      {     7,      11,       1,       7,      11,      1 }, //tu
      {     8,      11,       1,       8,      11,      1 }, //tu
      {     13,     33,       1,      13,      33,      1 }, //tu
      {     19,     67,       1,      19,      67,      1 }, //tu
      // clang-format on
  };

  // run c2c tests
  for (const auto &tc : test_cases_c2c) {
    printf("Testing n = %" PRId64 ", howmany = %" PRId64 "\n", tc.n,
           tc.howmany);

    auto ret =
        advanced_interface_test_sme<std::complex<float>, std::complex<float>>(
            tc);
    if (ret != EXIT_SUCCESS) {
      printf("Single-precision c2c failed for n = %" PRId64 "\n", tc.n);
      return ret;
    }
  }
}
