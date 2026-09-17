/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft.h"
#include "test_utils.hpp"

#include <array>
#include <random>
#include <vector>

namespace {

struct complex_f32 {
  float real;
  float imag;
};

std::vector<complex_f32> make_random(size_t n) {
  std::minstd_rand generator{0};
  std::uniform_real_distribution<float> distribution{-1.0F, 1.0F};
  std::vector<complex_f32> values(n);

  for (complex_f32 &value : values) {
    value = {distribution(generator), distribution(generator)};
  }
  return values;
}

bool matches(complex_f32 actual, complex_f32 expected, float tolerance) {
  return check(actual.real, expected.real, tolerance).success &&
         check(actual.imag, expected.imag, tolerance).success;
}

} // namespace

int main() {
  constexpr std::array<size_t, 22> sizes{
      2,  3,  4,   5,   7,   8,    11,   16,   21,   32,    47,
      64, 97, 128, 256, 509, 1024, 2053, 4096, 8191, 16384, 65536};

  for (const size_t n : sizes) {
    const std::vector<complex_f32> input = make_random(n);
    std::vector<complex_f32> reference(n);
    std::vector<complex_f32> exact_overlap = input;
    std::vector<complex_f32> partial_overlap_after = input;
    std::vector<complex_f32> partial_overlap_before = input;
    plfft_config_t *config = nullptr;
    plfft_plan_t *plan = nullptr;

    partial_overlap_after.resize(n + 1);
    partial_overlap_before.insert(partial_overlap_before.begin(),
                                  complex_f32{});

    REQUIRE(plfft_config_create(&config, n, PLFFT_FORWARD) == PLFFT_OK,
            "Error creating MAY_ALIAS config");
    plfft_config_set_io_alias(config, PLFFT_IO_MAY_ALIAS);
    REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_OK,
            "Error creating MAY_ALIAS plan");
    plfft_config_destroy(config);

    plfft_plan_execute(plan, input.data(), reference.data());
    plfft_plan_execute(plan, exact_overlap.data(), exact_overlap.data());
    plfft_plan_execute(plan, partial_overlap_after.data(),
                       partial_overlap_after.data() + 1);
    plfft_plan_execute(plan, partial_overlap_before.data() + 1,
                       partial_overlap_before.data());

    const float tolerance = default_tol<float>(n);
    for (size_t i = 0; i < n; ++i) {
      REQUIRE(matches(exact_overlap[i], reference[i], tolerance),
              "Exact-overlap result differs from reference");
      REQUIRE(matches(partial_overlap_after[i + 1], reference[i], tolerance),
              "Partial-overlap after differs from reference");
      REQUIRE(matches(partial_overlap_before[i], reference[i], tolerance),
              "Partial-overlap before differs from reference");
    }

    plfft_plan_destroy(plan);
  }
}
