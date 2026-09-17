/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_attrs.hpp"

#include <arm_sme.h>

namespace plfft {

template<int RealTile, int ImagTile>
PLFFT_ALWAYS_INLINE void accumulate_complex_outer_to_za(
    const svbool_t coeff_pg, const svbool_t data_pg,
    const svfloat32x2_t coeff_re_im,
    const svfloat32x2_t data_re_im) __arm_streaming __arm_inout("za") {
  svmopa_za32_f32_m(RealTile, coeff_pg, data_pg, svget2(coeff_re_im, 0),
                    svget2(data_re_im, 0));
  svmopa_za32_f32_m(ImagTile, coeff_pg, data_pg, svget2(coeff_re_im, 0),
                    svget2(data_re_im, 1));
  svmops_za32_f32_m(RealTile, coeff_pg, data_pg, svget2(coeff_re_im, 1),
                    svget2(data_re_im, 1));
  svmopa_za32_f32_m(ImagTile, coeff_pg, data_pg, svget2(coeff_re_im, 1),
                    svget2(data_re_im, 0));
}

} // namespace plfft
