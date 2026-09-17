/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "decimation.hpp"
#include "fft_internal_plan.hpp"
#include "plfft.h"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_util.hpp"

namespace plfft {

/**
 * Class to support using Bluestein's algorithm for prime n.
 *
 * @tparam Tx   The input type of the overall transform.
 * @tparam Ty   The output type of the overall transform.
 */
template<typename Tx, typename Ty>
struct bluestein {
  using real_t = remove_complex_t<Tx>;
  using cplx_t = std::complex<real_t>;

  int64_t n = 0;
  int64_t n_pad = 0;
  plfft_direction_t dir = (plfft_direction_t)0;

  pod_vector<cplx_t> a; // zero padding is required
  pod_vector<cplx_t> b; // zero padding is required
  pod_vector<cplx_t> b_conj;

  fft_internal_plan_ptr pf = nullptr; // Forwards plan
  fft_internal_plan_ptr pb = nullptr; // Backwards plan

  operator bool() const {
    return n != 0;
  }

#ifndef NO_LIBCPP
  inline std::string plan_to_string() const {
    std::ostringstream sstm;
    sstm << "(bluestein ";
    sstm << n << ' ';
    sstm << pf->plan_to_string() << ' ';
    sstm << pb->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP
};

template<typename Tx, typename Ty>
bluestein<Tx, Ty> create_bluestein(int64_t n, plfft_direction_t dir,
                                   double target_secs_total, double margin,
                                   bool want_sme);

} // end namespace plfft
