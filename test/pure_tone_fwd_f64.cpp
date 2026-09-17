/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "pure_tone_test.hpp"

#include <complex>
#include <vector>

int main() {
  const std::vector<PureToneTestCase> cases{
      // clang-format off
    //  n  howmany  istride  idist  ostride  odist  k0
    {  4,       5,       1,     4,       1,     4,  2 },
    {  5,      11,       1,     5,      11,     1,  0 },
    {  6,       1,       1,     7,       1,     8,  5 },
    {  7,     128,     128,     1,     128,     1,  1 },
#if TEST_JIT_ONLY_CONFIGS
    {  8,       4,       9,     1,       1,     8,  3 },
#endif
    { 13,      11,       1,    11,      13,     1,  7 },
      // clang-format on
  };

  int status = 0;
  for (const auto &c : cases) {
    status |= pure_tone_fwd_test<std::complex<double>>(c);
  }
  return status;
}
