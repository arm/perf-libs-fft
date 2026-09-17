/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "delta_constant_test.hpp"

#include <complex>
#include <vector>

int main() {
  const std::vector<TestCase> cases{
      // clang-format off
    //    n  howmany  istride   idist  ostride   odist
    {     4,       5,       1,       4,       1,      4 },
    {     5,      11,       1,       5,      11,      1 },
    {     6,       7,       1,       7,       1,      8 },
    {     7,     128,     128,       1,     128,      1 },
#if TEST_JIT_ONLY_CONFIGS
    {     8,       4,       9,       1,       1,      8 },
#endif
      // clang-format on
  };

  int status = 0;
  for (const auto &c : cases) {
    status |= delta_to_constant_test<std::complex<float>>(c);
  }
  return status;
}
