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
    // uu kernels:
    {  4,       5,       5,     1,       5,     1,  1 },
    {  5,      11,      12,     1,      11,     1,  4 },
    {  6,       7,       7,     1,       7,     1,  0 },
    {  7,       1,       7,     1,      11,     1,  3 },
    { 11,       7,      11,     1,      12,     1,  5 },
    { 19,       9,      20,     1,      21,     1, 13 },
    // Arbitrary kernels:
    {  4,       5,       1,     4,       1,     4,  2 },
    {  5,      11,       1,     5,      11,     1,  0 },
    {  6,       1,       1,     7,       1,     8,  5 },
#if TEST_JIT_ONLY_CONFIGS
    {  8,       4,       9,     1,       1,     8,  3 },
#endif
    { 13,      11,       1,    11,      13,     1,  7 },
      // clang-format on
  };

  int status = 0;
  for (const auto &c : cases) {
    status |= pure_tone_fwd_test<std::complex<float>>(
        c, &plfft::make_batched_1d_plan_sme<std::complex<float>,
                                            std::complex<float>>);
  }
  return status;
}
