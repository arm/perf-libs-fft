/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "compositor.hpp"
#include "exec_compositor.hpp"
#include "fft_internal_plan.hpp"
#include "plfft/unique_ptr.hpp"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"

#include <cstdlib>

namespace plfft {

/**
 * Going forward T1 and T2 will be used to specify the input and output types,
 * ie complex_double, complex_float, double & float,
 * T1 and T2 can be different for R2C and C2R
 */
template<typename T1, typename T2>
class central_plan_dft : public fft_internal_plan {
  const int64_t howmany_;
  const int64_t istride_;
  const int64_t idist_;
  const int64_t ostride_;
  const int64_t odist_;

  using composition_t = composition<T1, T2>;
  composition_t composition_;

public:
  central_plan_dft() = delete;
  central_plan_dft(central_plan_dft &&) = default;
  central_plan_dft(const central_plan_dft &) = delete;

  central_plan_dft(composition_t composition, int64_t howmany, int64_t istride,
                   int64_t idist, int64_t ostride, int64_t odist)
    : howmany_(howmany), istride_(istride), idist_(idist), ostride_(ostride),
      odist_(odist), composition_(std::move(composition)) {}

  inline void execute(const void *in, void *out) const override {
    ::plfft::execute(composition_, howmany_, (const T1 *)in, (T2 *)out,
                     istride_, ostride_, idist_, odist_);
  }

  inline void execute(int64_t howmany, const void *in,
                      void *out) const override {
    ::plfft::execute(composition_, howmany, (const T1 *)in, (T2 *)out, istride_,
                     ostride_, idist_, odist_);
  }

#ifndef NO_LIBCPP
  inline std::string plan_to_string() const override {
    return composition_to_string(composition_);
  }
#endif // NO_LIBCPP

  inline const composition_t *get_composition() const {
    return &composition_;
  }

  inline algo_flops flops() const override {
    // composition_ describes one transform.
    return composition_.flops() * howmany_;
  }
}; // class central_iface

template<typename T>
using central_plan_dft_c2c =
    central_plan_dft<add_complex_t<T>, add_complex_t<T>>;
template<typename T>
using central_plan_dft_r2c =
    central_plan_dft<remove_complex_t<T>, add_complex_t<T>>;
template<typename T>
using central_plan_dft_c2r =
    central_plan_dft<add_complex_t<T>, remove_complex_t<T>>;

template<typename T1, typename T2>
using central_plan_dft_ptr = plfft::unique_ptr<central_plan_dft<T1, T2>>;

} // namespace plfft
