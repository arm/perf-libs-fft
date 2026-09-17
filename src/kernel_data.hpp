/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "get_sve.hpp"
#include "kernel_function_pointers.hpp"
#include "kernel_registry_entry.hpp"
#include "plfft.h"
#include "plfft_complex.hpp"
#include "plfft_kernels.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_util.hpp"
#include "twiddle_layout.hpp"

#include <cstdint>
#include <optional>
#include <tuple>

namespace plfft {

template<typename FloatTypeX, typename FloatTypeY>
struct kernel_data_common {
  using FloatTypeW = add_complex_t<FloatTypeX>;

  kernel_registry_entry<plfft::fft_func_n_t<FloatTypeX, FloatTypeY>> ab_n;
  kernel_registry_entry<plfft::fft_func_t_t<FloatTypeW>> ab_t_dit;

  /// How to arrange twiddle factors to be passed to the kernels.
  twiddle_layout twid_layout;
};

template<typename FloatTypeX, typename FloatTypeY, typename = void>
struct kernel_data;

template<typename FloatTypeX, typename FloatTypeY>
struct kernel_data<FloatTypeX, FloatTypeY,
                   std::enable_if_t<is_c2c_v<FloatTypeX, FloatTypeY>>>
  : kernel_data_common<FloatTypeX, FloatTypeY> {
  using FloatTypeW = add_complex_t<FloatTypeX>;

  kernel_registry_entry<plfft::fft_func_t_t<FloatTypeW>> ac_t_dit;
};

template<typename FloatTypeX, typename FloatTypeY>
struct kernel_data<FloatTypeX, FloatTypeY,
                   std::enable_if_t<is_r2c_v<FloatTypeX, FloatTypeY>>>
  : kernel_data_common<FloatTypeX, FloatTypeY> {
  using FloatTypeW = add_complex_t<FloatTypeX>;

  kernel_registry_entry<plfft::fft_func_n_t<FloatTypeW, FloatTypeW>> ab_nfoh;
  kernel_registry_entry<plfft::fft_func_j_t<FloatTypeW>> ab_tfj_dit;
  kernel_registry_entry<plfft::fft_func_t_t<FloatTypeW>> ab_tfol_dit;

  kernel_registry_entry<plfft::fft_func_j_t<FloatTypeW>> ac_tfj_dit;
  kernel_registry_entry<plfft::fft_func_t_t<FloatTypeW>> ac_tfol_dit;
};

template<typename FloatTypeX, typename FloatTypeY>
struct kernel_data<FloatTypeX, FloatTypeY,
                   std::enable_if_t<is_c2r_v<FloatTypeX, FloatTypeY>>>
  : kernel_data_common<FloatTypeX, FloatTypeY> {
  using FloatTypeW = add_complex_t<FloatTypeX>;

  kernel_registry_entry<plfft::fft_func_in_t<FloatTypeW>> ab_nbih;
  kernel_registry_entry<plfft::fft_func_it_t<FloatTypeW>> ab_tbih_dif;
  kernel_registry_entry<plfft::fft_func_it_t<FloatTypeW>> ab_tbil_dif;

  kernel_registry_entry<plfft::fft_func_it_t<FloatTypeW>> ac_tbih_dif;
  kernel_registry_entry<plfft::fft_func_it_t<FloatTypeW>> ac_tbil_dif;
};

template<typename Tx, typename Ty>
std::optional<kernel_data<Tx, Ty>>
get_kernel_data(int64_t n, std::optional<int64_t> howmany,
                std::optional<int64_t> istride, std::optional<int64_t> ostride,
                std::optional<int64_t> idist, std::optional<int64_t> odist,
                plfft_direction_t dir, int strategy, order_kind order,
                bool want_sme);

template<typename Tx, typename Ty>
const pod_vector<int> &get_kernel_ns();

} // namespace plfft
