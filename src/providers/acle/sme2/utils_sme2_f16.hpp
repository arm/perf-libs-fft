/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_attrs.hpp"

#include <arm_sme.h>

namespace plfft {

PLFFT_ALWAYS_INLINE svfloat16_t
swap_pairs(svbool_t pred, const svfloat16_t value) __arm_streaming {
  svuint32_t u_value = svreinterpret_u32_f16(value);
  return svreinterpret_f16_u32(svrevh_u32_x(pred, u_value));
}

template<int Lane>
PLFFT_ALWAYS_INLINE svfloat16_t
get_interleaved_slice(svfloat32x4_t real_4slices_f32,
                      svfloat32x4_t imag_4slices_f32) __arm_streaming {
  svfloat32x2_t row_f32 = svcreate2_f32(svget4_f32(real_4slices_f32, Lane),
                                        svget4_f32(imag_4slices_f32, Lane));
  return svcvtn_f16_f32_x2(row_f32);
}

template<int RealTile, int ImagTile>
PLFFT_ALWAYS_INLINE void accumulate_complex_outer_to_za_f16(
    svbool_t coeff_pg, svbool_t data_pg, svfloat16_t coeff, svfloat16_t data,
    svbool_t pimag) __arm_streaming __arm_inout("za") {
  svfloat16_t data_conjugate = svneg_f16_m(data, pimag, data);
  svfloat16_t swapped_coeff = swap_pairs(coeff_pg, coeff);

  svmopa_za32_f16_m(RealTile, coeff_pg, data_pg, coeff, data_conjugate);
  svmopa_za32_f16_m(ImagTile, coeff_pg, data_pg, swapped_coeff, data);
}

} // namespace plfft
