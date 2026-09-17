/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "kernel_data.hpp"
#include "plfft_complex.hpp"
#include "plfft_kernels.hpp"

namespace plfft {

template<typename Tx, typename Ty, typename Tw, order_kind order,
         dist_types dist, out_mods = out_mods::om_none,
         in_mods = in_mods::im_none>
sme2_fft_func_n_t<Tx, Ty, Tw> *lookup_fft_func_n_acle_sme2(int64_t n) {
  return nullptr;
}

#ifdef PLFFT_ENABLE_SME2

#define DECLARE_ACLE_SME2_C2C_LOOKUP(T, Dist)                                  \
  template<>                                                                   \
  sme2_fft_func_n_t<T, T, T> *                                                 \
  lookup_fft_func_n_acle_sme2<T, T, T, order_kind::ORDER_NA, Dist,             \
                              out_mods::om_none, in_mods::im_none>(int64_t n);

// Half-precision c2c
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::uu)
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::tt)
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::tu)
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::ut)

// Single precision c2c
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::uu)
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::tt)
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::tu)
DECLARE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::ut)

template<>
sme2_fft_func_n_t<float, complex_float, complex_float> *
lookup_fft_func_n_acle_sme2<float, complex_float, complex_float,
                            order_kind::ORDER_NA, dist_types::tt,
                            out_mods::halfhi, in_mods::im_real>(int64_t n);

#undef DECLARE_ACLE_SME2_C2C_LOOKUP

#endif

} // namespace plfft
