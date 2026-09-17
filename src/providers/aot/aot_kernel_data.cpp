/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "cpu_features.hpp"
#include "kernel_data.hpp"
#include "kernel_lookup.hpp"
#include "kernel_provider_capabilities.hpp"
#include "plfft_assert.hpp"
#include "vector_size.hpp"

#include <algorithm>

namespace plfft {

bool kernel_provider_emulates_fp16() {
  return false;
}

template<typename Tx, typename Ty, typename Tw, typename = void>
struct kernel_data_generator;

template<typename Tx, typename Ty, typename Tw>
struct kernel_data_generator<Tx, Ty, Tw, std::enable_if_t<is_c2c_v<Tx, Ty>>> {
  template<dist_types dist>
  static void get(kernel_data<Tx, Ty> &ret, int64_t n, plfft_direction_t dir,
                  order_kind order, bool want_sve, bool want_sme) {
    switch (order) {
    case order_kind::ORDER_NA:
      ret.ab_n = {lookup_fft_func_n<Tx, Ty, order_kind::ORDER_NA, dist>(
                      n, dir, want_sve, want_sme),
                  {}};
      break;
    case order_kind::ORDER_AB:
      ret.ab_t_dit = {lookup_fft_func_t<Tw, order_kind::ORDER_AB, dist>(
                          n, dir, want_sve, want_sme),
                      {}};
      break;
    case order_kind::ORDER_AC:
      ret.ac_t_dit = {lookup_fft_func_t<Tw, order_kind::ORDER_AC, dist>(
                          n, dir, want_sve, want_sme),
                      {}};
      break;
    }
  }
};

template<typename Tx, typename Ty, typename Tw>
struct kernel_data_generator<Tx, Ty, Tw, std::enable_if_t<is_r2c_v<Tx, Ty>>> {
  template<dist_types dist>
  static void get(kernel_data<Tx, Ty> &ret, int64_t n, plfft_direction_t dir,
                  order_kind order, bool want_sve, bool want_sme) {
    switch (order) {
    case order_kind::ORDER_NA:
      ret.ab_n = {lookup_fft_func_n<Tx, Ty, order_kind::ORDER_NA, dist,
                                    out_mods::halfhi, in_mods::im_real>(
                      n, dir, want_sve, want_sme),
                  {}};
      ret.ab_nfoh = {
          lookup_fft_func_n<Tw, Tw, order_kind::ORDER_NA, dist,
                            out_mods::halfhi>(n, dir, want_sve, want_sme),
          {}};
      break;
    case order_kind::ORDER_AB:
      ret.ab_tfj_dit = {
          lookup_fft_func_j<Tw, order_kind::ORDER_AB, dist,
                            out_mods::conj_reverse>(n, dir, want_sve, want_sme),
          {}};
      ret.ab_tfol_dit = {
          lookup_fft_func_t<Tw, order_kind::ORDER_AB, dist, out_mods::halflo>(
              n, dir, want_sve, want_sme),
          {}};
      break;
    case order_kind::ORDER_AC:
      ret.ac_tfj_dit = {
          lookup_fft_func_j<Tw, order_kind::ORDER_AC, dist,
                            out_mods::conj_reverse>(n, dir, want_sve, want_sme),
          {}};
      ret.ac_tfol_dit = {
          lookup_fft_func_t<Tw, order_kind::ORDER_AC, dist, out_mods::halflo>(
              n, dir, want_sve, want_sme),
          {}};
      break;
    }
  }
};

template<typename Tx, typename Ty, typename Tw>
struct kernel_data_generator<Tx, Ty, Tw, std::enable_if_t<is_c2r_v<Tx, Ty>>> {
  template<dist_types dist>
  static void get(kernel_data<Tx, Ty> &ret, int64_t n, plfft_direction_t dir,
                  order_kind order, bool want_sve, bool want_sme) {
    switch (order) {
    case order_kind::ORDER_NA:
      ret.ab_n = {
          lookup_fft_func_n<Tx, Ty, order_kind::ORDER_NA, dist,
                            out_mods::om_real>(n, dir, want_sve, want_sme),
          {}};
      ret.ab_nbih = {
          lookup_fft_func_in<Tw, order_kind::ORDER_NA, dist, out_mods::om_none,
                             in_mods::im_halfhi>(n, dir, want_sve, want_sme),
          {}};
      break;
    case order_kind::ORDER_AB:
      if (n % 2 == 0) {
        ret.ab_tbil_dif = {
            lookup_fft_func_it<Tw, order_kind::ORDER_AB, dist,
                               out_mods::om_none, in_mods::im_halflo>(
                n, dir, want_sve, want_sme),
            {}};
      } else {
        ret.ab_tbih_dif = {
            lookup_fft_func_it<Tw, order_kind::ORDER_AB, dist,
                               out_mods::om_none, in_mods::im_halfhi>(
                n, dir, want_sve, want_sme),
            {}};
      }
      break;
    case order_kind::ORDER_AC:
      if (n % 2 == 0) {
        ret.ac_tbil_dif = {
            lookup_fft_func_it<Tw, order_kind::ORDER_AC, dist,
                               out_mods::om_none, in_mods::im_halflo>(
                n, dir, want_sve, want_sme),
            {}};
      } else {
        ret.ac_tbih_dif = {
            lookup_fft_func_it<Tw, order_kind::ORDER_AC, dist,
                               out_mods::om_none, in_mods::im_halfhi>(
                n, dir, want_sve, want_sme),
            {}};
      }
      break;
    }
  }
};

/// Returns the fft kernels for a particular n and plfft_direction_t.
template<typename Tx, typename Ty>
std::optional<kernel_data<Tx, Ty>>
get_kernel_data(int64_t n, std::optional<int64_t> howmany,
                std::optional<int64_t> istride, std::optional<int64_t> ostride,
                std::optional<int64_t> idist, std::optional<int64_t> odist,
                plfft_direction_t dir, int strategy, order_kind order,
                bool want_sme) {
  using Tw = add_complex_t<Tx>; // TODO support jcj kernels etc.

#ifndef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
  if constexpr (std::is_same_v<remove_complex_t<Tw>, half>) {
    return std::nullopt;
  }
#endif // __ARM_FEATURE_FP16_VECTOR_ARITHMETIC

  static_assert(std::is_same_v<remove_complex_t<Tx>, remove_complex_t<Ty>>,
                "Tx and Ty must have the same non-complex type");
  const auto &provider_ns = get_kernel_ns<Tx, Ty>();
  if (std::find(provider_ns.cbegin(), provider_ns.cend(), n) ==
      provider_ns.cend()) {
    return std::nullopt;
  }

  using Kgen = kernel_data_generator<Tx, Ty, Tw>;
  const bool want_sve = get_cpu_features().sve;

  kernel_data<Tx, Ty> ret;

  // check this first since we want to skip the howmany=1/uun early exit if
  // want_tu is true: we don't currently have a tun kernel
  bool want_tu = false;
  if constexpr (is_c2c_v<Tx, Ty> || is_r2c_v<Tx, Ty>) {
    if (want_sme && istride == 1 && odist == 1 && idist == n &&
        (is_c2c_v<Tx, Ty> || order == order_kind::ORDER_AB) &&
        // FP64 not currently supported by TU
        (sizeof(remove_complex_t<Tx>) == 4 ||
         sizeof(remove_complex_t<Tx>) == 2)) {
      want_tu = true;
    }
  }

  // uun kernels use Neon values here.
  ret.twid_layout.precision =
      runtime_precision_from_real_type<remove_complex_t<Tw>>();
  ret.twid_layout.want_premul = true;
  ret.twid_layout.interleave_factor = 1;

  bool want_uu = idist == 1 && odist == 1;
  const bool want_sme_uu_for_single_order_na =
      want_sme && order == order_kind::ORDER_NA && want_uu;
  const bool want_sme_uu_for_single_r2c_ab =
      want_sme && is_r2c_v<Tx, Ty> && order == order_kind::ORDER_AB && want_uu;

  if (!want_tu && howmany == 1 && !want_sme_uu_for_single_order_na &&
      !want_sme_uu_for_single_r2c_ab) {
    Kgen::template get<dist_types::uun>(ret, n, dir, order, want_sve, want_sme);
    return ret;
  }

  if constexpr (std::is_integral_v<remove_complex_t<Tx>>) {
    // fixed-point transforms always use Neon kernels
    ret.twid_layout.want_premul = true;
    ret.twid_layout.interleave_factor = 1;

  } else {
    bool want_sme_twids = want_sme && (want_uu || want_tu);
    if (want_sme_twids && order == order_kind::ORDER_AB &&
        (is_c2c_v<Tx, Ty> || is_r2c_v<Tx, Ty>)) {
      ret.twid_layout.want_premul = false;
      // C2C, TU and R2C AB SME kernels use interleaved twiddles so they can be
      // loaded consecutively, avoiding gathers.
      ret.twid_layout.interleave_factor =
          is_c2c_v<Tx, Ty> || is_r2c_v<Tx, Ty> || want_tu
              ? 64 / sizeof(Tw) // SME is fixed VL=512
              : 1;
    } else {
      if (want_sve) {
        ret.twid_layout.want_premul = false;
        if (order == order_kind::ORDER_AB) {
          ret.twid_layout.interleave_factor =
              vector_size_bytes(false) / sizeof(Tw);
        } else {
          ret.twid_layout.interleave_factor = 1;
        }
      } else {
        ret.twid_layout.want_premul = !want_sme || !want_uu;
        ret.twid_layout.interleave_factor = 1;
      }
    }
  }

  if (want_uu) {
    Kgen::template get<dist_types::uu>(ret, n, dir, order, want_sve, want_sme);
    return ret;
  }

  if (want_tu) {
    Kgen::template get<dist_types::tu>(ret, n, dir, order, want_sve, want_sme);
    return ret;
  }

  if (idist == 1) {
    Kgen::template get<dist_types::us>(ret, n, dir, order, want_sve, want_sme);
    return ret;
  }

  if (odist == 1) {
    Kgen::template get<dist_types::gu>(ret, n, dir, order, want_sve, want_sme);
    return ret;
  }

  Kgen::template get<dist_types::gs>(ret, n, dir, order, want_sve, want_sme);
  return ret;
}

#define GET_KERNEL_DATA(Tx, Ty)                                                \
  template std::optional<kernel_data<Tx, Ty>> get_kernel_data(                 \
      int64_t n, std::optional<int64_t> howmany,                               \
      std::optional<int64_t> istride, std::optional<int64_t> ostride,          \
      std::optional<int64_t> idist, std::optional<int64_t> odist,              \
      plfft_direction_t dir, int strategy, order_kind order, bool want_sme);

GET_KERNEL_DATA(half, std::complex<half>)
GET_KERNEL_DATA(std::complex<half>, half)
GET_KERNEL_DATA(std::complex<half>, std::complex<half>)
GET_KERNEL_DATA(float, std::complex<float>)
GET_KERNEL_DATA(std::complex<float>, float)
GET_KERNEL_DATA(std::complex<float>, std::complex<float>)
GET_KERNEL_DATA(double, std::complex<double>)
GET_KERNEL_DATA(std::complex<double>, double)
GET_KERNEL_DATA(std::complex<double>, std::complex<double>)
GET_KERNEL_DATA(int8_t, std::complex<int8_t>)
GET_KERNEL_DATA(std::complex<int8_t>, int8_t)
GET_KERNEL_DATA(std::complex<int8_t>, std::complex<int8_t>)
GET_KERNEL_DATA(int16_t, std::complex<int16_t>)
GET_KERNEL_DATA(std::complex<int16_t>, int16_t)
GET_KERNEL_DATA(std::complex<int16_t>, std::complex<int16_t>)

#undef GET_KERNEL_DATA

template<typename Tx, typename Ty>
const pod_vector<int> &get_kernel_ns() {
#ifndef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
  if constexpr (std::is_same_v<remove_complex_t<Tx>, half>) {
    THREAD_LOCAL const pod_vector<int> empty{};
    return empty;
  }
#endif // __ARM_FEATURE_FP16_VECTOR_ARITHMETIC

  if constexpr (std::is_integral_v<remove_complex_t<Tx>>) {
    // The thread local is to avoid issues with cxa_guards. Otherwise we could
    // have them as static and add other weak implementation of the cxa_* as
    // needed.
    THREAD_LOCAL const pod_vector<int> fixed_point_kernel_ns = {2, 4, 8, 16};
    return fixed_point_kernel_ns;
  } else {
    THREAD_LOCAL const pod_vector<int> floating_point_kernel_ns = {
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    return floating_point_kernel_ns;
  }
}

#define GET_KERNEL_NS(Tx, Ty)                                                  \
  template const pod_vector<int> &get_kernel_ns<Tx, Ty>();

GET_KERNEL_NS(half, std::complex<half>)
GET_KERNEL_NS(std::complex<half>, half)
GET_KERNEL_NS(std::complex<half>, std::complex<half>)
GET_KERNEL_NS(float, std::complex<float>)
GET_KERNEL_NS(std::complex<float>, float)
GET_KERNEL_NS(std::complex<float>, std::complex<float>)
GET_KERNEL_NS(double, std::complex<double>)
GET_KERNEL_NS(std::complex<double>, double)
GET_KERNEL_NS(std::complex<double>, std::complex<double>)
GET_KERNEL_NS(int8_t, std::complex<int8_t>)
GET_KERNEL_NS(std::complex<int8_t>, int8_t)
GET_KERNEL_NS(std::complex<int8_t>, std::complex<int8_t>)
GET_KERNEL_NS(int16_t, std::complex<int16_t>)
GET_KERNEL_NS(std::complex<int16_t>, int16_t)
GET_KERNEL_NS(std::complex<int16_t>, std::complex<int16_t>)

#undef GET_KERNEL_NS

} // end namespace plfft
