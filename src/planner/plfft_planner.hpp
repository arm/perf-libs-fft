/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft/fft_plan.hpp"

#include "benchmarker.hpp"
#include "planner.hpp"
#include "plfft_complex.hpp"
#include "policy.hpp"
#include "problem.hpp"
#include "wisdom.hpp"

namespace plfft {

template<typename Tx, typename Ty>
inline fft_plan_ptr make_planner_batched_1d_plan(
    int64_t n, const Tx *, Ty *, int64_t howmany, int64_t istride,
    int64_t idist, int64_t ostride, int64_t odist, plfft_direction_t dir,
    double target_secs_total, double margin, bool want_sme) {
  auto policy = plfft::policy::from_options(want_sme, target_secs_total);

  plfft::benchmarker bench{target_secs_total, margin};
  plfft::wisdom wisdom;
  plfft::factors factors;
  plfft::planner planner{policy, bench, wisdom, factors};
  const auto p =
      make_problem<Tx, Ty>(n, istride, ostride, howmany, idist, odist, dir);
  return planner.make_plan(p);
}

} // namespace plfft
