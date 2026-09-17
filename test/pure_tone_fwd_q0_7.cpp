/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "pure_tone_test.hpp"

#include <complex>
#include <cstdint>
#include <vector>

int main() {
  const std::vector<PureToneTestCase> cases{
      // clang-format off
      //    n  howmany  istride   idist  ostride   odist   k0
      {     4,       5,       1,       4,       1,      4,   2 },
      {     8,       3,       1,       8,       1,      8,   1 },
      {    16,       1,       1,      16,       1,     16,   4 },
      {    32,     128,     128,       1,     128,      1,   9 },
      {    64,       2,       1,      64,       1,     64,  17 },
      // clang-format on
  };

  int status = 0;
  for (const auto &c : cases) {
    status |= pure_tone_fwd_test<std::complex<int8_t>>(c);
  }
  return status;
}
