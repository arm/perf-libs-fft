/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_util.hpp"

namespace plfft {

/**
 * An encoding of different precisions available in the FFT interface,
 * this is needed to bridge the gap between compile-time template types
 * versus runtime code generation types.
 */
enum class runtime_precision { fp16 = 1, fp32, fp64, q0_7, q0_15 };

/**
 * Lookup the runtime precision corresponding to a particular floating
 * point type (i.e. half/float/double).
 */
template<typename T>
constexpr runtime_precision runtime_precision_from_real_type();

template<>
constexpr runtime_precision runtime_precision_from_real_type<half>() {
  return runtime_precision::fp16;
}

template<>
constexpr runtime_precision runtime_precision_from_real_type<float>() {
  return runtime_precision::fp32;
}

template<>
constexpr runtime_precision runtime_precision_from_real_type<double>() {
  return runtime_precision::fp64;
}

template<>
constexpr runtime_precision runtime_precision_from_real_type<int8_t>() {
  return runtime_precision::q0_7;
}

template<>
constexpr runtime_precision runtime_precision_from_real_type<int16_t>() {
  return runtime_precision::q0_15;
}

} // namespace plfft
