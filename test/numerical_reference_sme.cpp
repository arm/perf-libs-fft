/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "numerical_reference_test.hpp"

#include <cinttypes>
#include <complex>
#include <string>
#include <vector>

template<typename real_t>
int run_test(const TestCase &c, plfft_direction_t dir,
             const char *precision_name) {
  using complex_t = std::complex<real_t>;
  const real_t tolerance = default_tol<real_t>(c.n) * 2;
  const int status = numerical_reference_test<complex_t>(
      c, dir, &plfft::make_batched_1d_plan_sme<complex_t, complex_t>,
      tolerance);
  if (status != EXIT_SUCCESS) {
    const std::string dir_name = dir == PLFFT_FORWARD ? "forward" : "backward";
    printf("%s %s FFT numerical reference c2c failed for n = %" PRId64 "\n",
           precision_name, dir_name.c_str(), c.n);
  }
  return status;
}

int main() {
  const std::vector<TestCase> cases{
      // clang-format off
    //  n  howmany  istride  idist  ostride  odist
    {256,     256,       1,   256,       1,   256 }, // tt kernels
    {256,     256,     256,     1,     256,     1 }, // uu kernels
    { 32,      11,       1,    34,       1,    36 }, // tt
    { 48,      11,       1,    50,       1,    52 }, // tt
    { 64,      11,       1,    66,       1,    67 }, // tt
    { 80,      11,       1,    82,       1,    84 }, // tt
    { 96,      11,       1,    98,       1,   100 }, // tt
    {112,      11,       1,   114,       1,   116 }, // tt
    {128,      11,       1,   130,       1,   132 }, // tt
    {144,      11,       1,   146,       1,   148 }, // tt
    {160,      11,       1,   162,       1,   164 }, // tt
    {176,      11,       1,   178,       1,   180 }, // tt
    {192,      11,       1,   194,       1,   196 }, // tt
    {208,      11,       1,   210,       1,   212 }, // tt
    {224,      11,       1,   226,       1,   228 }, // tt
    {240,      11,       1,   242,       1,   244 }, // tt
    {256,      11,       1,   258,       1,   260 }, // tt
    { 32,      16,      18,     1,      19,     1 }, // uu
    { 48,      16,      18,     1,      19,     1 }, // uu
    { 64,      16,      18,     1,      19,     1 }, // uu
    { 80,      16,      18,     1,      19,     1 }, // uu
    { 96,      16,      18,     1,      19,     1 }, // uu
    {112,      16,      18,     1,      19,     1 }, // uu
    {128,      16,      18,     1,      19,     1 }, // uu
    {144,      16,      18,     1,      19,     1 }, // uu
    {160,      16,      18,     1,      19,     1 }, // uu
    {176,      16,      18,     1,      19,     1 }, // uu
    {192,      16,      18,     1,      19,     1 }, // uu
    {208,      16,      18,     1,      19,     1 }, // uu
    {224,      16,      18,     1,      19,     1 }, // uu
    {240,      16,      18,     1,      19,     1 }, // uu
    {256,      16,      18,     1,      19,     1 }, // uu
    { 32,       1,      18,     1,      19,     1 }, // uu tail
    { 80,       7,      18,     1,      19,     1 }, // uu tail
    {128,      15,      18,     1,      19,     1 }, // uu tail
    {192,      17,      19,     1,      21,     1 }, // uu tail
    {256,      31,      33,     1,      35,     1 }, // uu tail
    { 32,       1,       1,     1,      13,     1 }, // uu/tu overlap
    { 32,       1,      13,     1,       1,     1 }, // uu/ut overlap
    { 32,       1,       1,     1,       1,    35 }, // tt/ut overlap
    { 32,       1,       1,    35,       1,     1 }, // tt/tu overlap
    { 32,      11,       1,    32,      13,     1 }, // tu
    { 48,      11,       1,    48,      13,     1 }, // tu
    { 64,      11,       1,    64,      13,     1 }, // tu
    { 80,      11,       1,    80,      13,     1 }, // tu
    { 96,      11,       1,    96,      13,     1 }, // tu
    {112,      11,       1,   112,      13,     1 }, // tu
    {128,      11,       1,   128,      13,     1 }, // tu
    {144,      11,       1,   144,      13,     1 }, // tu
    {160,      11,       1,   160,      13,     1 }, // tu
    {176,      11,       1,   176,      13,     1 }, // tu
    {192,      11,       1,   192,      13,     1 }, // tu
    {208,      11,       1,   208,      13,     1 }, // tu
    {224,      11,       1,   224,      13,     1 }, // tu
    {240,      11,       1,   240,      13,     1 }, // tu
    {256,      11,       1,   256,      13,     1 }, // tu
    { 32,      13,      15,     1,       1,    35 }, // ut
    { 48,      13,      15,     1,       1,    51 }, // ut
    { 64,      13,      15,     1,       1,    67 }, // ut
    { 80,      13,      15,     1,       1,    83 }, // ut
    { 96,      13,      15,     1,       1,    99 }, // ut
    {112,      13,      15,     1,       1,   115 }, // ut
    {128,      13,      15,     1,       1,   131 }, // ut
    {144,      13,      15,     1,       1,   147 }, // ut
    {160,      13,      15,     1,       1,   163 }, // ut
    {176,      13,      15,     1,       1,   179 }, // ut
    {192,      13,      15,     1,       1,   195 }, // ut
    {208,      13,      15,     1,       1,   211 }, // ut
    {224,      13,      15,     1,       1,   227 }, // ut
    {240,      13,      15,     1,       1,   243 }, // ut
    {256,      13,      15,     1,       1,   259 }, // ut
      // clang-format on
  };
  constexpr plfft_direction_t directions[] = {PLFFT_FORWARD, PLFFT_BACKWARD};

  for (const auto &c : cases) {
    for (const auto dir : directions) {
      const int status = run_test<float>(c, dir, "Single-precision");
      if (status != EXIT_SUCCESS) {
        return status;
      }
    }
  }
  for (const auto &c : cases) {
    for (const auto dir : directions) {
      const int status = run_test<__fp16>(c, dir, "Half-precision");
      if (status != EXIT_SUCCESS) {
        return status;
      }
    }
  }
  return EXIT_SUCCESS;
}
