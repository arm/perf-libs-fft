## SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
##
## SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
<%include file="banner.txt.mako"/>\
#pragma once

#include "plfft_complex.hpp"
#include "plfft.h"
#include "plfft_kernels.hpp"
#include "kernel_data.hpp"

namespace plfft {

template<typename Tx, typename Ty, order_kind order, dist_types dist,
  out_mods = out_mods::om_none, in_mods = in_mods::im_none>
fft_func_n_t<Tx, Ty> *
lookup_fft_func_n(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme) {
  return nullptr;
}

template<typename Tw, order_kind order, dist_types dist,
  out_mods = out_mods::om_none, in_mods = in_mods::im_none>
fft_func_t_t<Tw> *
lookup_fft_func_t(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme) {
  return nullptr;
}

template<typename Tw, order_kind order, dist_types dist,
  out_mods = out_mods::om_none, in_mods = in_mods::im_none>
fft_func_j_t<Tw> *
lookup_fft_func_j(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme) {
  return nullptr;
}

template<typename Tw, order_kind order, dist_types dist,
  out_mods = out_mods::om_none, in_mods = in_mods::im_none>
fft_func_in_t<Tw> *
lookup_fft_func_in(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme) {
  return nullptr;
}

template<typename Tw, order_kind order, dist_types dist,
  out_mods = out_mods::om_none, in_mods = in_mods::im_none>
fft_func_it_t<Tw> *
lookup_fft_func_it(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme) {
  return nullptr;
}

% for lookup in config:
% if lookup.is_fp16:
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif
% if lookup.is_fixed_point:
#if PLFFT_ENABLE_FIXED_POINT
% endif
template<>
${lookup.fft_func_t} *
${lookup.name}(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme);
% if lookup.is_fixed_point:
#endif // PLFFT_ENABLE_FIXED_POINT
% endif
% if lookup.is_fp16:
#endif // __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif

% endfor

} // namespace plfft
