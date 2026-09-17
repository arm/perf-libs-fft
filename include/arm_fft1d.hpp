/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft.h"
#include "plfft/fft_plan.hpp"

#include <cstdint>

namespace plfft {

inline const char *api_version() {
  return "5";
}

template<typename T1, typename T2>
fft_plan_ptr make_batched_1d_plan(int64_t n0, int64_t howmany, int64_t istride,
                                  int64_t idist, int64_t ostride, int64_t odist,
                                  int sign, plfft_io_alias_t alias,
                                  plfft_r2r_kind_t r2r_kind,
                                  double target_secs_total, double margin);

template<typename T1, typename T2>
fft_plan_ptr make_batched_1d_plan(int64_t n0, const T1 *in, T2 *out,
                                  int64_t howmany, int64_t istride,
                                  int64_t idist, int64_t ostride, int64_t odist,
                                  int sign, plfft_r2r_kind_t r2r_kind,
                                  double target_secs_total, double margin);

template<typename T1, typename T2>
fft_plan_ptr make_batched_1d_plan(int64_t n, int64_t howmany, int64_t istride,
                                  int64_t idist, int64_t ostride, int64_t odist,
                                  plfft_direction_t direction,
                                  plfft_io_alias_t alias);

template<typename T1, typename T2>
fft_plan_ptr
make_batched_1d_plan_sme(int64_t n, int64_t howmany, int64_t istride,
                         int64_t idist, int64_t ostride, int64_t odist,
                         plfft_direction_t direction, plfft_io_alias_t alias);

template<typename T>
fft_plan_ptr
make_batched_1d_r2r_plan(int64_t n, int64_t howmany, int64_t istride,
                         int64_t idist, int64_t ostride, int64_t odist,
                         plfft_r2r_kind_t r2r_kind, plfft_io_alias_t alias);

void clean();

} // end namespace plfft
