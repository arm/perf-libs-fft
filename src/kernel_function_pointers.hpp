/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstdint>

namespace plfft {
template<typename Tx, typename Ty>
using fft_func_n_t = void(const Tx *X, Ty *Y, int64_t istride, int64_t ostride,
                          int64_t howmany, int64_t idist, int64_t odist);

template<typename Tw>
using fft_func_t_t = void(const Tw *X, Tw *Y, int64_t istride, int64_t ostride,
                          const void *W, int64_t howmany, int64_t idist,
                          int64_t odist);

template<typename Tw>
using fft_func_j_t = void(const Tw *X, Tw *Y, Tw *YY, int64_t istride,
                          int64_t ostride, const void *W, int64_t howmany,
                          int64_t idist, int64_t odist);

template<typename Tw>
using fft_func_in_t = void(const Tw *X, const Tw *XX, Tw *Y, int64_t istride,
                           int64_t ostride, int64_t howmany, int64_t idist,
                           int64_t odist);

template<typename Tw>
using fft_func_it_t = void(const Tw *X, const Tw *XX, Tw *Y, int64_t istride,
                           int64_t ostride, const void *W, int64_t howmany,
                           int64_t idist, int64_t odist);

template<typename Tx, typename Ty, typename Tw>
using sme2_fft_func_n_t = void(const Tx *X, Ty *Y, int64_t istride,
                               int64_t ostride, const Tw *W_n1, const Tw *W_n2,
                               const Tw *tw, int64_t howmany, int64_t idist,
                               int64_t odist, const int64_t n);

} // namespace plfft