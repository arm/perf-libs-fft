/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "decimation.hpp"
#include "kernel_data.hpp"
#include "level_data.hpp"
#include "plfft.h"
#include "plfft_complex.hpp"
#include "plfft_move_vector.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_statistics.hpp"
#include "plfft_util.hpp"

namespace plfft {

struct noncopyable {
  noncopyable() = default;
  noncopyable(const noncopyable &) = delete;
  noncopyable(noncopyable &&) = default;
  noncopyable &operator=(const noncopyable &) = delete;
  noncopyable &operator=(noncopyable &&) = default;
};

template<typename Tx, typename Ty>
struct composition {
  int64_t n;
  plfft_direction_t dir;
  int64_t nlevels;
  move_vector<level_data_ptr<Tx, Ty>> levels;
  statistics::normal_distribution estimate;

  // Disable copy constructors
#if defined(__clang__) || __GNUC__ > 8 || defined(_WIN32)
  [[PLFFT_NO_UNIQUE_ADDRESS]]
#endif
  noncopyable _{};

  algo_flops flops() const;
}; // struct composition

template<typename Tx, typename Ty>
std::string composition_to_string(const composition<Tx, Ty> &c);

/**
 * Builds the composition which will be used in a central_plan_dft when the
 * user calls execute.
 *
 * @param[in] n                  The problem size.
 * @param[in] howmany            The number of FFT transforms to be computed.
 * @param[in] istride            The distance between successive elements of the
 *                               input data.
 * @param[in] idist              The distance between the start of each of the
 *                               howmany transforms in the input data.
 * @param[in] ostride            The distance between successive elements of the
 *                               output data.
 * @param[in] odist              The distance between the start of each of the
 *                               howmany transforms in the output data.
 * @param[in] dir                The plfft_direction_t of the fft, either
 * forward or backwards.
 * @param[in] target_secs_total  The target time limit for patience when
 *                               auditioning plans.
 * @param[in] margin             A value in the range [0, 1] representing how
 *                               certain we must be about a new composition
 *                               being better than an existing one before using
 *                               that instead.
 * @param[in] allow_convolutions Do we want to allow either Bluestein or Raders?
 * @return  A pair of `(s, c)` where `s` is a boolean indicating success of
 *          creating the composition, and `c` is the composition itself.
 */
template<typename Tx, typename Ty>
std::pair<bool, composition<Tx, Ty>>
composite_init(int64_t n, int64_t howmany, int64_t istride, int64_t idist,
               int64_t ostride, int64_t odist, plfft_direction_t dir,
               double target_secs_total, double margin, bool allow_convolutions,
               bool want_sme);

} // namespace plfft
