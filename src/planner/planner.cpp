/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <cassert>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

#include "benchmarker.hpp"
#include "factorize.hpp"
#include "planner.hpp"
#include "plfft/unique_ptr.hpp"
#include "plfft_complex.hpp"
#include "strategies/bluestein.hpp"
#include "strategies/cooley_tukey.hpp"
#include "strategies/direct.hpp"
#ifdef PLFFT_ENABLE_SME2
#include "strategies/direct_sme2.hpp"
#endif
#include "strategies/fft1.hpp"
#include "strategies/loop.hpp"
#include "strategies/rader.hpp"
#include "strategy.hpp"
#include "wisdom.hpp"

namespace plfft {

namespace {

template<template<typename, typename> typename Strategy, typename Tx,
         typename Ty>
unique_ptr<strategy<Tx, Ty>> make_for_floating_point() {
  using real_t = remove_complex_t<std::remove_const_t<Tx>>;
  if constexpr (std::is_integral_v<real_t>) {
    return nullptr;
  } else {
    return make_unique<Strategy<Tx, Ty>>();
  }
}

template<typename Tx, typename Ty>
const unique_ptr<strategy<Tx, Ty>> strategies[] = {
    make_unique<direct<Tx, Ty>>(2),
    make_unique<direct<Tx, Ty>>(3),
    make_unique<direct<Tx, Ty>>(4),
    make_unique<direct<Tx, Ty>>(5),
    make_unique<direct<Tx, Ty>>(6),
    make_unique<direct<Tx, Ty>>(7),
    make_unique<direct<Tx, Ty>>(8),
    make_unique<direct<Tx, Ty>>(9),
    make_unique<direct<Tx, Ty>>(10),
    make_unique<direct<Tx, Ty>>(11),
    make_unique<direct<Tx, Ty>>(12),
    make_unique<direct<Tx, Ty>>(13),
    make_unique<direct<Tx, Ty>>(14),
    make_unique<direct<Tx, Ty>>(15),
    make_unique<direct<Tx, Ty>>(16),
    make_unique<direct<Tx, Ty>>(17),
    make_unique<direct<Tx, Ty>>(18),
    make_unique<direct<Tx, Ty>>(19),
    make_unique<direct<Tx, Ty>>(20),
    make_unique<direct<Tx, Ty>>(21),
    make_unique<direct<Tx, Ty>>(22),
    make_unique<direct<Tx, Ty>>(24),
    make_unique<direct<Tx, Ty>>(25),
    make_unique<direct<Tx, Ty>>(26),
    make_unique<direct<Tx, Ty>>(28),
    make_unique<direct<Tx, Ty>>(30),
    make_unique<direct<Tx, Ty>>(32),
    make_unique<direct<Tx, Ty>>(33),
    make_unique<direct<Tx, Ty>>(34),
    make_unique<direct<Tx, Ty>>(35),
    make_unique<direct<Tx, Ty>>(36),
    make_unique<direct<Tx, Ty>>(38),
    make_unique<direct<Tx, Ty>>(39),
    make_unique<direct<Tx, Ty>>(40),

#ifdef PLFFT_ENABLE_SME2
    make_unique<direct_sme2<Tx, Ty>>(32),
    make_unique<direct_sme2<Tx, Ty>>(48),
    make_unique<direct_sme2<Tx, Ty>>(64),
    make_unique<direct_sme2<Tx, Ty>>(80),
    make_unique<direct_sme2<Tx, Ty>>(96),
    make_unique<direct_sme2<Tx, Ty>>(112),
    make_unique<direct_sme2<Tx, Ty>>(128),
    make_unique<direct_sme2<Tx, Ty>>(144),
    make_unique<direct_sme2<Tx, Ty>>(160),
    make_unique<direct_sme2<Tx, Ty>>(176),
    make_unique<direct_sme2<Tx, Ty>>(192),
    make_unique<direct_sme2<Tx, Ty>>(208),
    make_unique<direct_sme2<Tx, Ty>>(224),
    make_unique<direct_sme2<Tx, Ty>>(240),
    make_unique<direct_sme2<Tx, Ty>>(256),
#endif

    make_unique<cooley_tukey_ab<Tx, Ty>>(40),
    make_unique<cooley_tukey_ab<Tx, Ty>>(39),
    make_unique<cooley_tukey_ab<Tx, Ty>>(38),
    make_unique<cooley_tukey_ab<Tx, Ty>>(36),
    make_unique<cooley_tukey_ab<Tx, Ty>>(35),
    make_unique<cooley_tukey_ab<Tx, Ty>>(34),
    make_unique<cooley_tukey_ab<Tx, Ty>>(33),
    make_unique<cooley_tukey_ab<Tx, Ty>>(32),
    make_unique<cooley_tukey_ab<Tx, Ty>>(30),
    make_unique<cooley_tukey_ab<Tx, Ty>>(28),
    make_unique<cooley_tukey_ab<Tx, Ty>>(26),
    make_unique<cooley_tukey_ab<Tx, Ty>>(25),
    make_unique<cooley_tukey_ab<Tx, Ty>>(24),
    make_unique<cooley_tukey_ab<Tx, Ty>>(22),
    make_unique<cooley_tukey_ab<Tx, Ty>>(21),
    make_unique<cooley_tukey_ab<Tx, Ty>>(20),
    make_unique<cooley_tukey_ab<Tx, Ty>>(19),
    make_unique<cooley_tukey_ab<Tx, Ty>>(18),
    make_unique<cooley_tukey_ab<Tx, Ty>>(17),
    make_unique<cooley_tukey_ab<Tx, Ty>>(16),
    make_unique<cooley_tukey_ab<Tx, Ty>>(15),
    make_unique<cooley_tukey_ab<Tx, Ty>>(14),
    make_unique<cooley_tukey_ab<Tx, Ty>>(13),
    make_unique<cooley_tukey_ab<Tx, Ty>>(12),
    make_unique<cooley_tukey_ab<Tx, Ty>>(11),
    make_unique<cooley_tukey_ab<Tx, Ty>>(10),
    make_unique<cooley_tukey_ab<Tx, Ty>>(9),
    make_unique<cooley_tukey_ab<Tx, Ty>>(8),
    make_unique<cooley_tukey_ab<Tx, Ty>>(7),
    make_unique<cooley_tukey_ab<Tx, Ty>>(6),
    make_unique<cooley_tukey_ab<Tx, Ty>>(5),
    make_unique<cooley_tukey_ab<Tx, Ty>>(4),
    make_unique<cooley_tukey_ab<Tx, Ty>>(3),
    make_unique<cooley_tukey_ab<Tx, Ty>>(2),

    make_unique<cooley_tukey_ac<Tx, Ty>>(40),
    make_unique<cooley_tukey_ac<Tx, Ty>>(39),
    make_unique<cooley_tukey_ac<Tx, Ty>>(38),
    make_unique<cooley_tukey_ac<Tx, Ty>>(36),
    make_unique<cooley_tukey_ac<Tx, Ty>>(35),
    make_unique<cooley_tukey_ac<Tx, Ty>>(34),
    make_unique<cooley_tukey_ac<Tx, Ty>>(33),
    make_unique<cooley_tukey_ac<Tx, Ty>>(32),
    make_unique<cooley_tukey_ac<Tx, Ty>>(30),
    make_unique<cooley_tukey_ac<Tx, Ty>>(28),
    make_unique<cooley_tukey_ac<Tx, Ty>>(26),
    make_unique<cooley_tukey_ac<Tx, Ty>>(25),
    make_unique<cooley_tukey_ac<Tx, Ty>>(24),
    make_unique<cooley_tukey_ac<Tx, Ty>>(22),
    make_unique<cooley_tukey_ac<Tx, Ty>>(21),
    make_unique<cooley_tukey_ac<Tx, Ty>>(20),
    make_unique<cooley_tukey_ac<Tx, Ty>>(19),
    make_unique<cooley_tukey_ac<Tx, Ty>>(18),
    make_unique<cooley_tukey_ac<Tx, Ty>>(17),
    make_unique<cooley_tukey_ac<Tx, Ty>>(16),
    make_unique<cooley_tukey_ac<Tx, Ty>>(15),
    make_unique<cooley_tukey_ac<Tx, Ty>>(14),
    make_unique<cooley_tukey_ac<Tx, Ty>>(13),
    make_unique<cooley_tukey_ac<Tx, Ty>>(12),
    make_unique<cooley_tukey_ac<Tx, Ty>>(11),
    make_unique<cooley_tukey_ac<Tx, Ty>>(10),
    make_unique<cooley_tukey_ac<Tx, Ty>>(9),
    make_unique<cooley_tukey_ac<Tx, Ty>>(8),
    make_unique<cooley_tukey_ac<Tx, Ty>>(7),
    make_unique<cooley_tukey_ac<Tx, Ty>>(6),
    make_unique<cooley_tukey_ac<Tx, Ty>>(5),
    make_unique<cooley_tukey_ac<Tx, Ty>>(4),
    make_unique<cooley_tukey_ac<Tx, Ty>>(3),
    make_unique<cooley_tukey_ac<Tx, Ty>>(2),

    make_for_floating_point<cooley_tukey_rader_ab, Tx, Ty>(),
    make_for_floating_point<cooley_tukey_rader_ac, Tx, Ty>(),
    make_for_floating_point<cooley_tukey_bluestein_ab, Tx, Ty>(),
    make_for_floating_point<cooley_tukey_bluestein_ac, Tx, Ty>(),
    make_for_floating_point<rader_strategy, Tx, Ty>(),
    make_for_floating_point<bluestein_strategy, Tx, Ty>(),
    make_unique<loop<Tx, Ty>>(),
    make_unique<fft1<Tx, Ty>>(),
};

} // namespace

int64_t factors::smallest_prime_factor(int64_t n) {
  if (auto *cached = smallest_prime_factors.find(n)) {
    return *cached;
  }

  return *smallest_prime_factors.insert(n, plfft::smallest_prime_factor(n));
}

bool factors::is_prime(int64_t n) {
  return n > 1 && smallest_prime_factor(n) == n;
}

planner::planner(plfft::policy policy_, plfft::benchmarker &bench_,
                 plfft::wisdom &wisdom_, plfft::factors &factors_)
  : policy(policy_), bench(bench_), wisdom(wisdom_), factors(factors_) {}

planner planner::without(plfft::policy x) const {
  return {policy - x, bench, wisdom, factors};
}

bool planner::has(plfft::policy x) const {
  return policy.contains(x);
}

int64_t planner::smallest_prime_factor(int64_t n) const {
  return factors.smallest_prime_factor(n);
}

bool planner::is_prime(int64_t n) const {
  return factors.is_prime(n);
}

fft_plan_ptr planner::make_plan(const problem &p) {
  switch (p.kind) {
  case transform_kind::c2c:
    switch (p.precision) {
    case runtime_precision::fp16:
      return make_plan_impl<std::complex<half>, std::complex<half>>(p);
    case runtime_precision::fp32:
      return make_plan_impl<std::complex<float>, std::complex<float>>(p);
    case runtime_precision::fp64:
      return make_plan_impl<std::complex<double>, std::complex<double>>(p);
#if PLFFT_ENABLE_FIXED_POINT
    case runtime_precision::q0_7:
      return make_plan_impl<std::complex<int8_t>, std::complex<int8_t>>(p);
    case runtime_precision::q0_15:
      return make_plan_impl<std::complex<int16_t>, std::complex<int16_t>>(p);
#else
    case runtime_precision::q0_7:
    case runtime_precision::q0_15:
      break;
#endif
    }
    break;
  case transform_kind::r2c:
    switch (p.precision) {
    case runtime_precision::fp16:
      return make_plan_impl<half, std::complex<half>>(p);
    case runtime_precision::fp32:
      return make_plan_impl<float, std::complex<float>>(p);
    case runtime_precision::fp64:
      return make_plan_impl<double, std::complex<double>>(p);
#if PLFFT_ENABLE_FIXED_POINT
    case runtime_precision::q0_7:
      return make_plan_impl<int8_t, std::complex<int8_t>>(p);
    case runtime_precision::q0_15:
      return make_plan_impl<int16_t, std::complex<int16_t>>(p);
#else
    case runtime_precision::q0_7:
    case runtime_precision::q0_15:
      break;
#endif
    }
    break;
  case transform_kind::c2r:
    switch (p.precision) {
    case runtime_precision::fp16:
      return make_plan_impl<std::complex<half>, half>(p);
    case runtime_precision::fp32:
      return make_plan_impl<std::complex<float>, float>(p);
    case runtime_precision::fp64:
      return make_plan_impl<std::complex<double>, double>(p);
#if PLFFT_ENABLE_FIXED_POINT
    case runtime_precision::q0_7:
      return make_plan_impl<std::complex<int8_t>, int8_t>(p);
    case runtime_precision::q0_15:
      return make_plan_impl<std::complex<int16_t>, int16_t>(p);
#else
    case runtime_precision::q0_7:
    case runtime_precision::q0_15:
      break;
#endif
    }
    break;
  }

  assert(false && "unsupported transform type or runtime precision");
  std::abort();
}

template<typename Tx, typename Ty>
fft_plan_ptr planner::make_plan_impl(const problem &p) {
  constexpr auto num_strategies = std::size(strategies<Tx, Ty>);

  // see if a plan has already been saved for this problem
  if (auto entry = wisdom.lookup(p, policy)) {
    auto [saved_strategy_index, saved_policy] = *entry;
    fft_plan_ptr plan = nullptr;
    planner saved_planner{saved_policy, bench, wisdom, factors};

    // wisdom keeps track of the strategy that was used to create the plan
    if (saved_strategy_index < num_strategies) {
      const auto &strategy = strategies<Tx, Ty>[saved_strategy_index];
      if (strategy) {
        plan = strategy->make_plan(p, saved_planner);
      }
    }

    // this should never happen!
    if (!plan) {
      assert(false && "failed to reconstruct plan from wisdom");
      std::abort();
    }

    // return plan recalled from wisdom, no benchmarking needed
    return plan;
  }

  // track the best plan seen so far
  fft_plan_ptr best_plan = nullptr;
  std::optional<std::size_t> best_strategy_index;
  std::optional<benchmarker::result> best_result;
  const bool stop_after_first_candidate = has(ALLOW_PRUNING);

  // try strategies in order until we run out of time
  std::size_t strategy_index = 0;
  for (const auto &strategy : strategies<Tx, Ty>) {
    if (!strategy) {
      strategy_index++;
      continue;
    }

    // stop if we've run out of time, provided there is already a valid plan
    if (!stop_after_first_candidate && best_plan && bench.timed_out()) {
      break;
    }

    auto candidate = strategy->make_plan(p, *this);
    if (candidate) {
      // small hack to skip benchmarking
      if (stop_after_first_candidate) {
        best_plan = std::move(candidate);
        best_strategy_index = strategy_index;
        break;
      }

      // measure the candidate plan against the best plan so far
      const auto result = bench.measure<Tx, Ty>(p, *candidate, best_result);

      // if time ran out before measuring, return a best-effort plan
      if (!result) {
        if (!best_plan) {
          best_plan = std::move(candidate);
          best_strategy_index = strategy_index;
        }
        break;
      }

      // replace the best plan when the candidate plan is better
      if (!best_plan || !best_result || bench.prefers(*result, *best_result)) {
        best_plan = std::move(candidate);
        best_strategy_index = strategy_index;
        best_result = *result;
      }
    }

    // try next strategy
    strategy_index++;
  }

  // store wisdom about the best strategy for this problem
  if (best_strategy_index) {
    wisdom.update(p, {*best_strategy_index, policy});
  }

  return best_plan;
}

} // namespace plfft
