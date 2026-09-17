/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "kernel_lookup.hpp"
#include "kernels.hpp"

#include <type_traits>

using namespace plfft;

#ifdef PLFFT_ENABLE_SME2

namespace plfft {

namespace {

template<typename T, dist_types dist>
sme2_fft_func_n_t<T, T, T> *lookup_c2c_kernel(int64_t n) {
  if (n >= 32 && n <= 256 && n % 16 == 0) {
    if constexpr (std::is_same_v<T, complex_half>) {
      switch (dist) {
      case dist_types::tt:
        return plfft_16x_jjjn_tt_sme2;
      case dist_types::uu:
        return plfft_16x_jjjn_uu_sme2;
      case dist_types::tu:
        return plfft_16x_jjjn_tu_sme2;
      case dist_types::ut:
        return plfft_16x_jjjn_ut_sme2;
      }
    } else if constexpr (std::is_same_v<T, complex_float>) {
      switch (dist) {
      case dist_types::tt:
        return plfft_16x_cccn_tt_sme2;
      case dist_types::uu:
        return plfft_16x_cccn_uu_sme2;
      case dist_types::tu:
        return plfft_16x_cccn_tu_sme2;
      case dist_types::ut:
        return plfft_16x_cccn_ut_sme2;
      }
    }
  }
  return nullptr;
}

} // namespace

template<>
sme2_fft_func_n_t<float, complex_float, complex_float> *
lookup_fft_func_n_acle_sme2<float, complex_float, complex_float,
                            order_kind::ORDER_NA, dist_types::tt,
                            out_mods::halfhi, in_mods::im_real>(int64_t n) {
  return n == 256 ? plfft_256_sccnoh_tt_sme2 : nullptr;
}

#define DEFINE_ACLE_SME2_C2C_LOOKUP(T, Dist)                                   \
  template<>                                                                   \
  sme2_fft_func_n_t<T, T, T> *                                                 \
  lookup_fft_func_n_acle_sme2<T, T, T, order_kind::ORDER_NA, Dist,             \
                              out_mods::om_none, in_mods::im_none>(            \
      int64_t n) {                                                             \
    return lookup_c2c_kernel<T, Dist>(n);                                      \
  }

DEFINE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::uu)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::tt)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::tu)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_half, dist_types::ut)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::uu)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::tt)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::tu)
DEFINE_ACLE_SME2_C2C_LOOKUP(complex_float, dist_types::ut)

#undef DEFINE_ACLE_SME2_C2C_LOOKUP

} // namespace plfft

#endif
