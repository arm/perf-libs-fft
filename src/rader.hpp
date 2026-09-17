/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "fft_internal_plan.hpp"
#include "plfft.h"
#include "plfft_complex.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_util.hpp"

#include <optional>

namespace plfft {

/**
 * Class to support using Rader's algorithm for prime n.
 *
 * @tparam Tx   The input type of the overall transform.
 * @tparam Ty   The output type of the overall transform.
 */
template<typename Tx, typename Ty>
struct rader {
  using Tw = add_complex_t<Tx>;
  int64_t n = 0;

  int64_t g = 0;

  fft_internal_plan_ptr pf = nullptr;
  fft_internal_plan_ptr pb = nullptr;
  bool use_direct_kernel = false;
  int64_t work_stride = 0;

  pod_vector<Tw> b;

  pod_vector<int64_t> gmul_fw_perm;
  pod_vector<int64_t> ginvmul_fw_perm;
  pod_vector<int64_t> ginvmul_bw_perm;

  operator bool() const {
    return n != 0;
  }

#ifndef NO_LIBCPP
  inline std::string plan_to_string() const {
    std::ostringstream sstm;
    sstm << "(rader ";
    sstm << n << ' ';
    sstm << pf->plan_to_string() << ' ';
    sstm << pb->plan_to_string() << ')';
    return std::move(sstm).str();
  }
#endif // NO_LIBCPP
};

template<typename Tx, typename Ty>
std::optional<rader<Tx, Ty>>
create_rader(int64_t n, int64_t howmany, plfft_direction_t dir,
             double target_secs_total, double margin, bool use_direct_kernel,
             bool want_sme);

} // end namespace plfft
