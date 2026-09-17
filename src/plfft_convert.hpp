/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <arm_neon.h>

namespace plfft {

template<typename T>
struct float_type {
  using type = T;
};

template<>
struct float_type<int8_t> {
  using type = float;
};

template<>
struct float_type<int16_t> {
  using type = float;
};

template<>
struct float_type<int32_t> {
  using type = float;
};

template<typename T>
using float_type_t = typename float_type<T>::type;

// Helpers to convert from floating-point to fixed-point
template<typename ToType>
struct convert_to;

// fp32 -> Q0.7 (8-bit)
template<>
struct convert_to<int8_t> {
  int8_t operator()(const float x) const {
    return vqmovnh_s16(vqmovns_s32(vcvts_n_s32_f32(x, 7)));
  }
};

// fp32 -> Q0.15 (16-bit)
template<>
struct convert_to<int16_t> {
  int16_t operator()(const float x) const {
    return vqmovns_s32(vcvts_n_s32_f32(x, 15));
  }
};

// fp32 -> Q0.31 (32-bit)
template<>
struct convert_to<int32_t> {
  int32_t operator()(const float x) const {
    return vcvts_n_s32_f32(x, 31);
  }
};

} // namespace plfft
