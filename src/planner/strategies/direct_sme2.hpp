/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "cpu_features.hpp"
#include "kernel_function_pointers.hpp"
#include "planner/planner.hpp"
#include "planner/problem.hpp"
#include "planner/strategy.hpp"
#include "plfft/fft_plan.hpp"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"
#include "providers/acle/acle_provider.hpp"
#include "providers/acle/sme2/utils_twiddle_factors.hpp"

#include <cassert>
#include <cinttypes>
#ifndef NO_LIBCPP
#include <sstream>
#endif // NO_LIBCPP

namespace plfft {

template<typename Tx, typename Ty>
struct direct_sme2_plan : public fft_plan {
  constexpr static int radix = 16;

  direct_sme2_plan(int64_t n_, int64_t istride_, int64_t ostride_,
                   int64_t howmany_, int64_t idist_, int64_t odist_,
                   plfft_direction_t dir_)
    : n(n_), istride(istride_), ostride(ostride_), howmany(howmany_),
      idist(idist_), odist(odist_), kernel(nullptr), flops_{},
      dft_matrix_n1(radix * radix), dft_matrix_n2(), twiddle(radix * radix) {
    const auto data = acle_provider::get_sme2_direct_kernel_data<Tx, Ty>(
        n, howmany_, istride, ostride, idist, odist);
    // If we get here, we must have a valid kernel.
    assert(data);
    kernel = data->kernel.get_text_ptr();
    flops_ = data->kernel.flops * howmany_;
    // Calculate the matrices for the direct_sme2_plan
    calculate_fft_coefficients<Tw>(dft_matrix_n1.data(), matrix_layout::NORMAL,
                                   radix, radix, radix, dir_);

    const int radix2 = n / 16;
    dft_matrix_n2.resize(radix2 * radix2);
    calculate_fft_coefficients<Tw>(dft_matrix_n2.data(), matrix_layout::NORMAL,
                                   radix2, radix2, radix2, dir_);
    matrix_layout layout = matrix_layout::NORMAL;
    if constexpr (is_c2c_v<Tx, Ty> &&
                  std::is_same_v<remove_complex_t<Tx>, float>) {
      if (data->dist == dist_types::uu || data->dist == dist_types::ut) {
        layout = matrix_layout::INTERLEAVE_ROW_PAIRS;
      }
    }
    calculate_fft_coefficients<Tw>(twiddle.data(), layout, n, radix2, radix,
                                   dir_);
  }

  void execute(const void *in, void *out) const override {
    const auto *x = static_cast<const Tx *>(in);
    auto *y = static_cast<Ty *>(out);
    kernel(x, y, istride, ostride, dft_matrix_n1.data(), dft_matrix_n2.data(),
           twiddle.data(), howmany, idist, odist, n);
  }
#ifndef NO_LIBCPP
  std::string plan_to_string() const override {
    std::ostringstream sstm;
    sstm << "(direct_sme2 " << n << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP
  algo_flops flops() const override {
    return flops_;
  }

private:
  using Tw = add_complex_t<Tx>;
  int64_t n;
  int64_t istride, ostride;
  int64_t howmany, idist, odist;
  sme2_fft_func_n_t<Tx, Ty, Tw> *kernel;
  algo_flops flops_;
  pod_vector<Tw> dft_matrix_n1;
  pod_vector<Tw> dft_matrix_n2;
  pod_vector<Tw> twiddle;
};

template<typename Tx, typename Ty>
struct direct_sme2 : public strategy<Tx, Ty> {
  explicit direct_sme2(int n_) : n(n_) {}

  fft_plan_ptr make_plan(const problem &p,
                         plfft::planner &planner) const override {
    using plan_t = direct_sme2_plan<Tx, Ty>;
    if (!is_applicable(p, planner)) {
      return nullptr;
    }
    [[maybe_unused]] const auto [kind, prec, np, istride, ostride, howmany,
                                 idist, odist, dir] = p;
    return plfft::make_unique<plan_t>(np, istride, ostride, howmany, idist,
                                      odist, dir);
  }

private:
  const int64_t n;

  bool is_applicable(const problem &p, const planner &planner) const {

    [[maybe_unused]] const auto [kind, prec, np, istride, ostride, howmany,
                                 idist, odist, dir] = p;
    if (!get_cpu_features().sme2) {
      return false;
    }

    /* The precision-specific transform, length and layout rules live with the
     * ACLE provider. */
    if (!(is_c2c_v<Tx, Ty> || is_r2c_v<Tx, Ty>) ||
        (prec != runtime_precision::fp32 && prec != runtime_precision::fp16) ||
        !planner.has(ALLOW_SME2)) {
      return false;
    }

    if (np != n) {
      return false;
    }
    return acle_provider::get_sme2_direct_kernel_data<Tx, Ty>(
               np, howmany, istride, ostride, idist, odist)
        .has_value();
  }
};
} // namespace plfft
