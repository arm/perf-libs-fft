/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once
#include "plfft.h"
#include "plfft_complex.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>

namespace plfft {

enum class matrix_layout { NORMAL, INTERLEAVE_ROW_PAIRS };

template<typename Tw>
void calculate_fft_coefficients(Tw *matrix, const matrix_layout layout,
                                const int64_t n, const int64_t rows,
                                const int64_t cols,
                                const plfft_direction_t direction) {
  using real_Tw = remove_complex_t<Tw>;
  for (int32_t r = 0; r < rows; r++) {
    for (int32_t c = 0; c < cols; c++) {
      const auto angle = 2 * std::numbers::pi * (c * r) / n;
      const real_Tw re = static_cast<real_Tw>(std::cos(angle));
      const real_Tw im =
          static_cast<int>(direction) * static_cast<real_Tw>(std::sin(angle));
      int32_t rr = r;
      int32_t cc = c;
      if (layout == matrix_layout::INTERLEAVE_ROW_PAIRS) {
        const int32_t even = r & ~1;
        const int32_t odd = even + 1;
        const int32_t half = cols / 2;
        const int32_t block = c / half;
        const int32_t pos = c % half;
        rr = even + block;
        cc = 2 * pos + (r == odd ? 1 : 0);
      }
      const int32_t i = (rr * cols + cc);
      matrix[i] = Tw(re, im);
    }
  }
}
} // namespace plfft
