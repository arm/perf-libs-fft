/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "arm_fft1d.hpp"
#include "batched_1d_plan.hpp"
#include "central_direct_plan_dft.hpp"
#include "central_plan_dft.hpp"
#include "factorize.hpp"
#include "fft_config.hpp"
#include "fft_internal_plan.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "planner/plfft_planner.hpp"
#include "plfft_util.hpp"
#include "r2r_plan.hpp"

#include <cassert>

namespace plfft {

template<typename Tx, typename Ty>
static inline void fft1(const Tx *x, Ty *y, int64_t, int64_t, int64_t howmany,
                        int64_t idist, int64_t odist) {
  for (int i = 0; i < howmany; i++) {
    if constexpr (is_c2r_v<Tx, Ty>) {
      y[i * odist] = std::real(x[i * idist]);
    } else {
      y[i * odist] = x[i * idist];
    }
  }
}

template<typename T1, typename T2>
static inline fft_internal_plan_ptr
make_1d_fft1_plan(const T1 *in, T2 *out, int64_t howmany, int64_t idist,
                  int64_t odist) {
  auto kernel = &fft1<T1, T2>;
  auto flops = algo_flops{};
  return plfft::make_unique<central_direct_plan_dft<T1, T2>>(
      1, kernel, flops, howmany, 1, idist, 1, odist);
}

template<typename T1, typename T2>
static inline fft_internal_plan_ptr
make_1d_direct_plan(int64_t n0, const T1 *in, T2 *out, int64_t howmany,
                    int64_t istride, int64_t idist, int64_t ostride,
                    int64_t odist, plfft_direction_t dir, bool want_sme) {
  auto strategy = config::get_kernel_strategy();

  auto kernels =
      get_kernel_data<T1, T2>(n0, howmany, istride, ostride, idist, odist, dir,
                              strategy, order_kind::ORDER_NA, want_sme);
  if (kernels) {
    if (!kernels->ab_n) {
      return nullptr;
    }
    auto *ab_n = kernels->ab_n.get_text_ptr();
    return plfft::make_unique<central_direct_plan_dft<T1, T2>>(
        n0, ab_n, kernels->ab_n.flops, howmany, istride, idist, ostride, odist);
  }
  return nullptr;
}

template<typename T1, typename T2, typename Alt = std::nullptr_t>
static inline fft_internal_plan_ptr
make_1d_plan(int64_t n0, const T1 *in, T2 *out, int64_t howmany,
             int64_t istride, int64_t idist, int64_t ostride, int64_t odist,
             plfft_direction_t dir, double target_secs_total, double margin,
             bool allow_convolutions, bool want_sme) {
  static_assert(std::is_same_v<remove_complex_t<T1>, remove_complex_t<T2>>);

  auto [success, comp] = composite_init<T1, T2>(
      n0, howmany, istride, idist, ostride, odist, dir, target_secs_total,
      margin, allow_convolutions, want_sme);
  if (!success) {
    return nullptr;
  }

  return plfft::make_unique<central_plan_dft<T1, T2>>(
      std::move(comp), howmany, istride, idist, ostride, odist);
}

template<typename T1, typename T2>
static inline fft_internal_plan_ptr
make_legacy_1d_plan(int64_t n0, const T1 *in, T2 *out, int64_t howmany,
                    int64_t istride, int64_t idist, int64_t ostride,
                    int64_t odist, plfft_direction_t dir,
                    double target_secs_total, double margin, bool want_sme) {
  // if this is a plan for an n=1 fft, use the fft1 kernel.
  if (n0 == 1) {
    return make_1d_fft1_plan(in, out, howmany, idist, odist);
  }

  auto direct_plan = make_1d_direct_plan(n0, in, out, howmany, istride, idist,
                                         ostride, odist, dir, want_sme);
  if (direct_plan) {
    return direct_plan;
  }

  return make_1d_plan(n0, in, out, howmany, istride, idist, ostride, odist, dir,
                      target_secs_total, margin, true, want_sme);
}

template<typename T1, typename T2>
fft_1d_plan_ptr inline make_1d_plan(int64_t n0, const T1 *in, T2 *out,
                                    int64_t howmany, int64_t istride,
                                    int64_t idist, int64_t ostride,
                                    int64_t odist, int sign,
                                    plfft_r2r_kind_t r2r_kind,
                                    double target_secs_total, double margin,
                                    bool want_sme) {
  static_assert(std::is_same_v<remove_complex_t<T1>, remove_complex_t<T2>>);

  if (n0 < 1) {
    return nullptr;
  }

  if constexpr (is_r2r_v<T1, T2>) {
    if (n0 == 1 && r2r_kind == PLFFT_R2R_DCT_1) {
      // DCT_1/REDFT00 transforms, are logically N=2*(n-1), so n=1 is undefined.
      return nullptr;
    }
    if (want_sme) {
      // R2R plans do not support SME
      return nullptr;
    }
    return make_r2r_plan_1d<T1>(n0, r2r_kind, howmany, istride, idist, ostride,
                                odist, target_secs_total, margin);
  } else {
    auto dir = static_cast<plfft_direction_t>(sign);

// TODO remove legacy once new planner supports multithreaded execution
#ifdef _OPENMP
    return make_legacy_1d_plan(n0, in, out, howmany, istride, idist, ostride,
                               odist, dir, target_secs_total, margin, want_sme);
#else
    return make_planner_batched_1d_plan(n0, in, out, howmany, istride, idist,
                                        ostride, odist, dir, target_secs_total,
                                        margin, want_sme);
#endif
  }
}

template<typename T1, typename T2>
inline fft_plan_ptr
make_batched_1d_plan(int64_t n, const T1 *in, T2 *out, bool inplace,
                     int64_t howmany, int64_t istride, int64_t idist,
                     int64_t ostride, int64_t odist, int sign,
                     plfft_r2r_kind_t r2r_kind, double target_secs_total,
                     double margin, bool want_sme = false) {
  if (in != nullptr && out != nullptr) {
    assert((static_cast<const void *>(in) == static_cast<const void *>(out)) ==
           inplace);
  }

  if constexpr (is_c2r_v<T1, T2>) {
    assert(static_cast<plfft_direction_t>(sign) == PLFFT_BACKWARD);
  }
  if constexpr (is_r2c_v<T1, T2>) {
    assert(static_cast<plfft_direction_t>(sign) == PLFFT_FORWARD);
  }

#ifndef _OPENMP
  if (is_c2c_v<T1, T2> || !inplace) {
    return make_1d_plan(n, in, out, howmany, istride, idist, ostride, odist,
                        sign, r2r_kind, target_secs_total, margin, want_sme);
  }
#endif

  const bool create_and_reorder_buffer = !is_c2c_v<T1, T2> && inplace;

  auto istride_1d_plan = istride;
  auto idist_1d_plan = idist;
  if (howmany > 1 && create_and_reorder_buffer) {
    /*
      The transform is in_place and not c2c. Ahead of computing the
      transform, we will construct a buffer that separates the data for each
      transform into different portions of the vector. In that buffer, the data
      for each individual transform will be contiguous but the distance between
      each transform in the length of the each transform in the batch, n.
    */
    istride_1d_plan = 1;
    idist_1d_plan = n;
  }

  auto plan_1d =
      make_1d_plan(n, in, out, howmany, istride_1d_plan, idist_1d_plan, ostride,
                   odist, sign, r2r_kind, target_secs_total, margin, want_sme);

  if (!plan_1d) {
    return nullptr;
  }

  return plfft::make_unique<batched_1d_plan<T1, T2>>(
      n, howmany, istride, ostride, idist, odist, create_and_reorder_buffer,
      std::move(plan_1d));
}

template<typename T1, typename T2>
fft_plan_ptr make_batched_1d_plan(int64_t n0, int64_t howmany, int64_t istride,
                                  int64_t idist, int64_t ostride, int64_t odist,
                                  int sign, plfft_io_alias_t alias,
                                  plfft_r2r_kind_t r2r_kind,
                                  double target_secs_total, double margin) {
  const bool inplace = alias == PLFFT_IO_MAY_ALIAS;
  return make_batched_1d_plan<T1, T2>(n0, nullptr, nullptr, inplace, howmany,
                                      istride, idist, ostride, odist, sign,
                                      r2r_kind, target_secs_total, margin);
}

template<typename T1, typename T2>
inline fft_plan_ptr
make_batched_1d_plan(int64_t n, const T1 *in, T2 *out, int64_t howmany,
                     int64_t istride, int64_t idist, int64_t ostride,
                     int64_t odist, int sign, plfft_r2r_kind_t r2r_kind,
                     double target_secs_total, double margin) {
  const bool inplace =
      static_cast<const void *>(in) == static_cast<const void *>(out);
  return make_batched_1d_plan(n, in, out, inplace, howmany, istride, idist,
                              ostride, odist, sign, r2r_kind, target_secs_total,
                              margin);
}

template<typename T1, typename T2>
inline fft_plan_ptr
make_batched_1d_plan(int64_t n, int64_t howmany, int64_t istride, int64_t idist,
                     int64_t ostride, int64_t odist,
                     plfft_direction_t direction, plfft_io_alias_t alias,
                     bool want_sme) {
  const bool inplace = alias == PLFFT_IO_MAY_ALIAS;
  return make_batched_1d_plan<T1, T2>(
      n, nullptr, nullptr, inplace, howmany, istride, idist, ostride, odist,
      (int)direction, (plfft_r2r_kind_t)0, 0, 0, want_sme);
}

template<typename T1, typename T2>
inline fft_plan_ptr
make_batched_1d_plan(int64_t n, int64_t howmany, int64_t istride, int64_t idist,
                     int64_t ostride, int64_t odist,
                     plfft_direction_t direction, plfft_io_alias_t alias) {
  return make_batched_1d_plan<T1, T2>(n, howmany, istride, idist, ostride,
                                      odist, direction, alias, false);
}

#ifdef PLFFT_ENABLE_SME
template<typename T1, typename T2>
inline fft_plan_ptr
make_batched_1d_plan_sme(int64_t n, int64_t howmany, int64_t istride,
                         int64_t idist, int64_t ostride, int64_t odist,
                         plfft_direction_t direction, plfft_io_alias_t alias) {
  return make_batched_1d_plan<T1, T2>(n, howmany, istride, idist, ostride,
                                      odist, direction, alias, true);
}
#endif

template<typename T>
fft_plan_ptr
make_batched_1d_r2r_plan(int64_t n, int64_t howmany, int64_t istride,
                         int64_t idist, int64_t ostride, int64_t odist,
                         plfft_r2r_kind_t r2r_kind, plfft_io_alias_t alias) {
  const bool inplace = alias == PLFFT_IO_MAY_ALIAS;
  return make_batched_1d_plan<T, T>(n, nullptr, nullptr, inplace, howmany,
                                    istride, idist, ostride, odist, 0, r2r_kind,
                                    0, 0);
}

} // namespace plfft
