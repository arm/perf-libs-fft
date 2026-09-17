/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "planner/planner.hpp"
#include "planner/strategy.hpp"
#include "plfft/fft_plan.hpp"
#include "plfft_complex.hpp"
#ifndef NO_LIBCPP
#include <string>
#endif // NO_LIBCPP
#include "planner/problem.hpp"

namespace plfft {

template<typename Tx, typename Ty>
struct fft1_plan : public fft_plan {
  fft1_plan(int64_t howmany_, int64_t idist_, int64_t odist_)
    : howmany(howmany_), idist(idist_), odist(odist_) {}

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    for (int64_t h = 0; h < howmany; h++) {
      if constexpr (is_c2r_v<Tx, Ty>) {
        y[h * odist] = real(x[h * idist]);
      } else {
        y[h * odist] = x[h * idist];
      }
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    return "(direct 1)";
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return {};
  }

private:
  int64_t howmany, idist, odist;
};

template<typename Tx, typename Ty>
struct fft1 : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p, plfft::planner &) const override {
    using plan_t = fft1_plan<Tx, Ty>;

    if (!is_applicable(p)) {
      return nullptr;
    }

    return plfft::make_unique<plan_t>(p.howmany, p.idist, p.odist);
  }

private:
  bool is_applicable(const problem &p) const {
    return (
        /* correctness constraints */
        p.n == 1);
  }
};

} // namespace plfft
