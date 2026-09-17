/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "compositor.hpp"
#include "fft_internal_plan.hpp"
#include "plfft/unique_ptr.hpp"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"

#include <cstdlib>
#include <sstream>

namespace plfft {

/**
 * Going forward T1 and T2 will be used to specify the input and output types,
 * ie complex_double, complex_float, double & float,
 * T1 and T2 can be different for R2C and C2R
 */
template<typename T1, typename T2>
class central_direct_plan_dft : public fft_internal_plan {
  const int64_t howmany_;
  const int64_t istride_;
  const int64_t idist_;
  const int64_t ostride_;
  const int64_t odist_;
  fft_func_n_t<T1, T2> *ab_n_;
  int64_t n_;
  algo_flops flops_n_;

public:
  central_direct_plan_dft() = delete;
  central_direct_plan_dft(central_direct_plan_dft &&) = default;
  central_direct_plan_dft(const central_direct_plan_dft &) = delete;

  central_direct_plan_dft(int64_t n, decltype(ab_n_) ab_n,
                          decltype(flops_n_) flops_n, int64_t howmany,
                          int64_t istride, int64_t idist, int64_t ostride,
                          int64_t odist)
    : howmany_(howmany), istride_(istride), idist_(idist), ostride_(ostride),
      odist_(odist), ab_n_(ab_n), n_(n), flops_n_(flops_n) {}

  inline void execute(const void *in, void *out) const override {
    (*ab_n_)((const T1 *)in, (T2 *)out, istride_, ostride_, howmany_, idist_,
             odist_);
  }

  inline void execute(int64_t howmany, const void *in,
                      void *out) const override {
    (*ab_n_)((const T1 *)in, (T2 *)out, istride_, ostride_, howmany, idist_,
             odist_);
  }

#ifndef NO_LIBCPP
  inline std::string plan_to_string() const override {
    std::stringstream sstm;
    sstm << "(direct " << n_ << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP
  inline algo_flops flops() const override {
    // flops_n_ describes one transform.
    return flops_n_ * howmany_;
  }
}; // class central_direct_plan_dft

template<typename T>
using central_direct_plan_dft_c2c =
    central_direct_plan_dft<add_complex_t<T>, add_complex_t<T>>;
template<typename T>
using central_direct_plan_dft_r2c =
    central_direct_plan_dft<remove_complex_t<T>, add_complex_t<T>>;
template<typename T>
using central_direct_plan_dft_c2r =
    central_direct_plan_dft<add_complex_t<T>, remove_complex_t<T>>;

template<typename T1, typename T2>
using central_direct_plan_dft_ptr =
    plfft::unique_ptr<central_direct_plan_dft<T1, T2>>;

} // namespace plfft
