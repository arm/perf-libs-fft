/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "bluestein.hpp"
#include "factorize.hpp"
#include "fft_config.hpp"
#include "generate_twiddles.hpp"
#include "kernel_data.hpp"
#include "level_ab_n.hpp"
#include "level_ab_t_c2c.hpp"
#include "level_ab_t_c2r.hpp"
#include "level_ab_t_r2c.hpp"
#include "level_abx_t_c2c.hpp"
#include "level_ac_t_c2c.hpp"
#include "level_ac_t_c2r.hpp"
#include "level_ac_t_r2c.hpp"
#include "level_bluestein.hpp"
#include "level_rader.hpp"
#include "plfft.h"
#include "plfft/unique_ptr.hpp"
#include "rader.hpp"

namespace plfft {

template<typename Tx, typename Ty>
static level_data_ptr<Tx, Ty>
make_level_rader(const int64_t n, const int64_t n1, const int64_t n2,
                 const int64_t howmany, const int64_t istride,
                 const int64_t ostride, const int64_t idist,
                 const int64_t odist, const plfft_direction_t dir,
                 const bool want_twiddles, rader<Tx, Ty> r) {
  assert(r);
  using Tw = add_complex_t<Tx>;
  if (want_twiddles) {
    const auto *twids = generate_twiddles<Tw>(dir, n2, n1, false, 1);
    return plfft::make_unique<level_rader_t<Tx, Ty>>(
        n, n1, n2, howmany, istride, ostride, idist, odist, twids,
        std::move(r));
  }
  return plfft::make_unique<level_rader_n<Tx, Ty>>(
      n, n1, n2, howmany, istride, ostride, idist, odist, std::move(r));
}

template<typename Tx, typename Ty>
static level_data_ptr<Tx, Ty>
make_level_bluestein(const int64_t n, const int64_t n1, const int64_t n2,
                     const int64_t howmany, const int64_t istride,
                     const int64_t ostride, const int64_t idist,
                     const int64_t odist, const plfft_direction_t dir,
                     const bool want_twiddles, bluestein<Tx, Ty> bs) {
  assert(bs);
  using Tw = add_complex_t<Tx>;
  if (want_twiddles) {
    const auto *twids = generate_twiddles<Tw>(dir, n2, n1, false, 1);
    return plfft::make_unique<level_bluestein_t<Tx, Ty>>(
        n, n1, n2, howmany, istride, ostride, idist, odist, twids,
        std::move(bs));
  }
  return plfft::make_unique<level_bluestein_n<Tx, Ty>>(
      n, n1, n2, howmany, istride, ostride, idist, odist, std::move(bs));
}

template<level_type LT, typename Tx, typename Ty>
static std::optional<level_data_ptr<Tx, Ty>>
make_level_data_direct(const int64_t n, const int64_t n1, const int64_t n2,
                       const int64_t howmany, const int64_t istride,
                       const int64_t ostride, const int64_t idist,
                       const int64_t odist, const plfft_direction_t dir,
                       const bool want_twiddles, const bool want_sme) {
  if (want_twiddles) {
    if constexpr (LT == level_type::INTERNAL) {
      return plfft::make_unique<level_ac_t<Tx, Ty>>(
          n, n1, n2, howmany, istride, ostride, idist, odist, dir, want_sme);
    } else {
      if (want_sme) {
        // Use abx for SME - uses ab twiddle kernel for all subtransforms,
        // including the one where twiddle factors are all one. This is
        // necessary as we cannot use uun with SME, and using tu instead is
        // inefficient
        if constexpr (is_c2c_v<Tx, Ty>) {
          // abx level is currently only implemented for c2c
          return plfft::make_unique<level_abx_t<Tx, Ty>>(
              n, n1, n2, howmany, istride, ostride, idist, odist, dir,
              want_sme);
        }
      }
      return plfft::make_unique<level_ab_t<Tx, Ty>>(
          n, n1, n2, howmany, istride, ostride, idist, odist, dir, want_sme);
    }
  }
  return plfft::make_unique<level_ab_n<Tx, Ty>>(
      n, n1, n2, howmany, istride, ostride, idist, odist, dir, want_sme);
}

template<level_type LT, typename Tx, typename Ty>
static std::optional<level_data_info>
make_level_data(level_data_ptr<Tx, Ty> *out, const int64_t n, const int64_t n1,
                const int64_t n2, const int64_t hm_lev, const int64_t howmany,
                const int64_t istride, const int64_t idist,
                const int64_t ostride, const int64_t odist,
                const plfft_direction_t dir, const double target_secs_total,
                const double margin, const bool allow_raders,
                const bool allow_bluestein, const bool want_twiddles,
                const bool want_sme) {

  auto kernel_factors = get_kernel_ns<Tx, Ty>();
  if (is_base_n(n1, kernel_factors)) {
    auto maybe_lev = make_level_data_direct<LT, Tx, Ty>(
        n, n1, n2, howmany, istride, ostride, idist, odist, dir, want_twiddles,
        want_sme);
    if (!maybe_lev) {
      return std::nullopt;
    }
    *out = std::move(*maybe_lev);
    return {{false}};
  }
  // if we're recursively planning from within Rader's or Bluestein, make
  // sure we don't end up in a recursive call, since it will be slow!
  if (!allow_raders && !allow_bluestein) {
    return std::nullopt;
  }

  // Rader's and Bluestein support only floating-point types.
  if constexpr (std::is_integral_v<remove_complex_t<Tx>>) {
    return std::nullopt;
  } else {
    using Tw = add_complex_t<Tx>;
    auto use_base_kernel = is_base_case<Tw, Tw>(n1 - 1, kernel_factors);

    // Choose correct howmany to pass into rader, depending on level type.
    // For single level, it is the overall howmany of the transform.
    // For non-twiddled level, it is n2 * hm_lev.
    // For twiddled level, it is n2 (or n2 / 2 + 1 for C2R) because we loop over
    // hm_lev.
    // This is used for vectorized kernel calls in rader when using base
    // kernels.
    auto hm = LT == level_type::SINGLE ? howmany
              : !want_twiddles         ? n2 * hm_lev
              : is_c2r_v<Tx, Ty>       ? n2 / 2 + 1
                                       : n2;

    auto maybe_r = allow_raders
                       ? create_rader<Tx, Ty>(n1, hm, dir, target_secs_total,
                                              margin, use_base_kernel, want_sme)
                       : std::nullopt;
    if (maybe_r) {
      *out = make_level_rader<Tx, Ty>(n, n1, n2, howmany, istride, ostride,
                                      idist, odist, dir, want_twiddles,
                                      std::move(*maybe_r));
      return {{true}};
    } else {
      auto bs = create_bluestein<Tx, Ty>(n1, dir, target_secs_total, margin,
                                         want_sme);
      *out = make_level_bluestein<Tx, Ty>(n, n1, n2, howmany, istride, ostride,
                                          idist, odist, dir, want_twiddles,
                                          std::move(bs));
      return {{false}};
    }
  }
}

} // namespace plfft
