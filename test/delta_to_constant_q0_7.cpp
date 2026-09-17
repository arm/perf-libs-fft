/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "delta_constant_test.hpp"

#include <complex>
#include <cstdint>
#include <vector>

int main() {
  const std::vector<TestCase> cases{
      // clang-format off
      //    n  howmany  istride   idist  ostride   odist
      {     4,       5,       1,       4,       1,      4 },
      {     8,       7,       1,       8,       1,      8 },
      {    16,       3,       1,      16,       2,     32 },
      {    64,       1,     128,       1,     128,      1 },
      // clang-format on
  };

  int status = 0;
  for (const auto &c : cases) {
    status |= delta_to_constant_test<std::complex<int8_t>>(c);
  }
  return status;
}
