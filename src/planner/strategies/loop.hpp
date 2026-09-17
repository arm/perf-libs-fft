/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "planner/planner.hpp"
#include "planner/strategy.hpp"
#include "plfft/fft_plan.hpp"
#include <cinttypes>
#include <utility>
#ifndef NO_LIBCPP
#include <sstream>
#endif // NO_LIBCPP
#include "planner/problem.hpp"

namespace plfft {

template<typename Tx, typename Ty>
class loop_plan : public fft_plan {
public:
  loop_plan(int64_t howmany_, int64_t idist_, int64_t odist_,
            fft_plan_ptr inner_)
    : howmany(howmany_), idist(idist_), odist(odist_),
      inner(std::move(inner_)) {}

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    for (int64_t i = 0; i < howmany; i++) {
      inner->execute(x + i * idist, y + i * odist);
    }
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(loop " << howmany << ' ';
    sstm << inner->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return inner->flops() * howmany;
  }

private:
  int64_t howmany, idist, odist;
  fft_plan_ptr inner;
};

template<typename Tx, typename Ty>
struct loop : public strategy<Tx, Ty> {
  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = loop_plan<Tx, Ty>;

    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;

    auto inner = planner.make_plan(
        make_problem<Tx, Ty>(n, istride, ostride, 1, 0, 0, dir));
    if (!inner) {
      return nullptr;
    }

    return plfft::make_unique<plan_t>(howmany, idist, odist, std::move(inner));
  }

private:
  bool is_applicable(const problem &p, const planner &) const {
    return (
        /* profitability constraints (heuristics) */
        p.n > 1 && p.howmany > 1);
  }
};

} // namespace plfft
