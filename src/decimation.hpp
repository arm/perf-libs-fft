/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_complex.hpp"

namespace plfft {

/**
 * Indicate which algorithm is to be used in the compositor.
 * c2c can use either (but in practice we always use time since it
 * is usually faster); r2c must use time; c2r must use frequency.
 * e.g. see: https://math.mit.edu/~stevenj/gdft.pdf
 */
enum class decimation { time = 0, frequency };

template<typename T1, typename T2>
inline constexpr decimation get_decimation() {
  if constexpr (is_c2c_v<T1, T2> || is_r2c_v<T1, T2>) {
    return decimation::time;
  } else {
    return decimation::frequency;
  }
}

template<typename T1, typename T2>
inline constexpr bool is_dit_v = get_decimation<T1, T2>() == decimation::time;

} // namespace plfft
