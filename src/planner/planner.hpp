/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft/fft_plan.hpp"
#include "plfft_static_hash_map.hpp"
#include "policy.hpp"

#include <cstdint>
#include <functional>

namespace plfft {

class benchmarker;
class wisdom;
struct problem;

// cache factor checks so planning avoids repeated trial division
class factors {
public:
  int64_t smallest_prime_factor(int64_t n);
  bool is_prime(int64_t n);

private:
  static_hash_map<int64_t, int64_t, 64, std::hash<int64_t>>
      smallest_prime_factors;
};

class planner {
public:
  planner(plfft::policy policy, plfft::benchmarker &bench,
          plfft::wisdom &wisdom, plfft::factors &factors);

  planner without(plfft::policy x) const;
  bool has(plfft::policy x) const;
  int64_t smallest_prime_factor(int64_t n) const;
  bool is_prime(int64_t n) const;

  fft_plan_ptr make_plan(const problem &p);

private:
  template<typename Tx, typename Ty>
  fft_plan_ptr make_plan_impl(const problem &p);

  const plfft::policy policy;
  plfft::benchmarker &bench;
  plfft::wisdom &wisdom;
  plfft::factors &factors;
};

} // namespace plfft
