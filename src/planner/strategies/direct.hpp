/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "factorize.hpp"
#include "kernel_data.hpp"
#include "kernel_function_pointers.hpp"
#include "planner/planner.hpp"
#include "planner/problem.hpp"
#include "planner/strategy.hpp"
#include "plfft/fft_plan.hpp"

#include <cassert>
#include <cinttypes>

#ifndef NO_LIBCPP
#include <sstream>
#endif // NO_LIBCPP

namespace plfft {

template<typename Tx, typename Ty>
struct direct_plan : public fft_plan {
  direct_plan(int64_t n_, int64_t istride_, int64_t ostride_, int64_t howmany_,
              int64_t idist_, int64_t odist_, plfft_direction_t dir_,
              bool want_sme)
    : n(n_), istride(istride_), ostride(ostride_), howmany(howmany_),
      idist(idist_), odist(odist_), kernel(nullptr), flops_{} {
    const auto data =
        get_kernel_data<Tx, Ty>(n_, howmany_, istride, ostride, idist, odist,
                                dir_, 0, order_kind::ORDER_NA, want_sme);
    // provider contract: every n advertised by get_kernel_ns has kernel data
    assert(data);
    kernel = data->ab_n.get_text_ptr();
    flops_ = data->ab_n.flops * howmany_;
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    kernel(x, y, istride, ostride, howmany, idist, odist);
  }

#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(direct " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP

  algo_flops flops() const override {
    return flops_;
  }

private:
  int64_t n;
  int64_t istride, ostride;
  int64_t howmany, idist, odist;

  fft_func_n_t<Tx, Ty> *kernel;
  algo_flops flops_;
};

template<typename Tx, typename Ty>
struct direct : public strategy<Tx, Ty> {
  explicit direct(int n_) : n(n_) {}

  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = direct_plan<Tx, Ty>;

    if (!is_applicable(p, planner)) {
      return nullptr;
    }

    const bool want_sme = planner.has(ALLOW_SME);
    const auto [_, prec, np, istride, ostride, howmany, idist, odist, dir] = p;
    return plfft::make_unique<plan_t>(np, istride, ostride, howmany, idist,
                                      odist, dir, want_sme);
  }

private:
  const int n;

  bool is_applicable(const problem &p, const planner &) const {
    const auto ns = get_kernel_ns<Tx, Ty>();

    return (
        /* correctness constraints */
        p.n == n && is_base_n(p.n, ns));
  }
};

} // namespace plfft
