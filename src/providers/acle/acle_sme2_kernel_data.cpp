/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "acle_provider.hpp"
#include "kernel_data.hpp"
#include "kernel_lookup.hpp"
#include "plfft_assert.hpp"

namespace plfft {

template<typename Tx, typename Ty, dist_types dist,
         out_mods out_mod = out_mods::om_none,
         in_mods in_mod = in_mods::im_none>
std::optional<acle_sme2_direct_kernel_data<Tx, Ty>>
lookup_sme2_direct_kernel_data(int64_t n) {
  using Tw = add_complex_t<Tx>;
  const kernel_registry_entry<sme2_fft_func_n_t<Tx, Ty, Tw>> kernel{
      lookup_fft_func_n_acle_sme2<Tx, Ty, Tw, order_kind::ORDER_NA, dist,
                                  out_mod, in_mod>(n),
      {}};

  if (!kernel) {
    return std::nullopt;
  }

  return acle_sme2_direct_kernel_data<Tx, Ty>{kernel, dist};
}

template<typename Tx, typename Ty>
std::optional<acle_sme2_direct_kernel_data<Tx, Ty>>
acle_provider::get_sme2_direct_kernel_data(int64_t n, int64_t howmany,
                                           int64_t istride, int64_t ostride,
                                           int64_t idist, int64_t odist) {
#ifdef PLFFT_ENABLE_SME2
  if constexpr (std::is_same_v<Tx, float> &&
                std::is_same_v<Ty, complex_float>) {
    if (n == 256 && istride == 1 && ostride == 1 && howmany > 0 &&
        howmany % 4 == 0) {
      return lookup_sme2_direct_kernel_data<Tx, Ty, dist_types::tt,
                                            out_mods::halfhi, in_mods::im_real>(
          n);
    }
  }
  if constexpr (std::is_same_v<Tx, Ty> && (std::is_same_v<Tx, complex_float> ||
                                           std::is_same_v<Tx, complex_half>)) {
    const bool want_uu = idist == 1 && odist == 1;
    const bool want_tt = istride == 1 && ostride == 1;
    const bool want_tu = istride == 1 && odist == 1;
    const bool want_ut = idist == 1 && ostride == 1;

    // Lets make sure we are in one of the supported complex layout cases.
    const int matching_layouts = want_uu + want_tt + want_tu + want_ut;
    if (matching_layouts == 0) {
      return std::nullopt;
    }

    // Multiple layouts may only match if howmany is 1, otherwise user
    // input is invalid. We have no way to fail elegantly at this point
    // so allow this to fall through to the first matching option, which
    // may or not be what the user meant.
    assert(matching_layouts == 1 || howmany == 1);

    // For c2c SME2 kernels we support lengths between 32 and 256 that are
    // multiples of 16.
    if (n >= 32 && n <= 256 && n % 16 == 0) {
      if (want_uu) {
        return lookup_sme2_direct_kernel_data<Tx, Ty, dist_types::uu>(n);
      }
      if (want_tt) {
        return lookup_sme2_direct_kernel_data<Tx, Ty, dist_types::tt>(n);
      }
      if (want_tu) {
        return lookup_sme2_direct_kernel_data<Tx, Ty, dist_types::tu>(n);
      }
      if (want_ut) {
        return lookup_sme2_direct_kernel_data<Tx, Ty, dist_types::ut>(n);
      }
    }
  }
#else
  assert(false &&
         "SME2 ACLE kernel requested but PLFFT_ENABLE_SME2 is disabled");
#endif
  return std::nullopt;
}

#define ACLE_GET_SME2_DIRECT_KERNEL_DATA(Tx, Ty)                               \
  template std::optional<acle_sme2_direct_kernel_data<Tx, Ty>>                 \
  acle_provider::get_sme2_direct_kernel_data<Tx, Ty>(                          \
      int64_t n, int64_t howmany, int64_t istride, int64_t ostride,            \
      int64_t idist, int64_t odist);

ACLE_GET_SME2_DIRECT_KERNEL_DATA(half, std::complex<half>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<half>, half)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<half>, std::complex<half>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(float, std::complex<float>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<float>, float)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<float>, std::complex<float>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(double, std::complex<double>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<double>, double)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<double>, std::complex<double>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(int8_t, std::complex<int8_t>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<int8_t>, int8_t)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<int8_t>, std::complex<int8_t>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(int16_t, std::complex<int16_t>)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<int16_t>, int16_t)
ACLE_GET_SME2_DIRECT_KERNEL_DATA(std::complex<int16_t>, std::complex<int16_t>)

#undef ACLE_GET_SME2_DIRECT_KERNEL_DATA

} // namespace plfft
