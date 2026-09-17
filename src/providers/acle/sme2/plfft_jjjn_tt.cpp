/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft_assert.hpp"
#include "plfft_complex.hpp"
#include "plfft_util.hpp"
#include "utils_sme2_f16.hpp"

#include <arm_sme.h>

namespace plfft {

template<int Lane>
PLFFT_ALWAYS_INLINE svfloat16_t twiddle_slice(
    svbool_t pg, svfloat32x4_t real_4slices_f32, svfloat32x4_t imag_4slices_f32,
    svfloat16x4_t twiddle_4rows) __arm_streaming {
  svfloat32x2_t row_f32 = svcreate2_f32(svget4_f32(real_4slices_f32, Lane),
                                        svget4_f32(imag_4slices_f32, Lane));
  svfloat16_t row_f16 = svcvtn_f16_f32_x2(row_f32);
  svfloat16_t result = svdup_f16(0.0F);
  result =
      svcmla_f16_x(pg, result, row_f16, svget4_f16(twiddle_4rows, Lane), 0);
  return svcmla_f16_x(pg, result, row_f16, svget4_f16(twiddle_4rows, Lane), 90);
}

/*
  Given one or two TT rows, this function calculates the 16-long FFTs of
  stride = root. Tiles za0,za1 are used for the first row while za2,za3 are
  used for the second row when UnrollTwo = true.

  The DFT matrix row is the MOPA row operand and the root-strided input row is
  the column operand. This orientation matches the FP16 uu kernel and lets both
  kernels share the same complex outer-product helper.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void calculate_16_stride_root(
    svbool_t p_root, svbool_t p_data, svcount_t pntrue, svbool_t pimag,
    const float16_t *input_row, const float16_t *w_ptr, const int64_t skip,
    const int64_t stride) __arm_streaming __arm_inout("za") {
  const float16_t *input_next_row = nullptr;
  if constexpr (UnrollTwo) {
    input_next_row = input_row + skip;
  }

  const int64_t offset = svcnth();

  for (int b = 0; b < 4; ++b) {
    svfloat16x4_t w16_4rows = svld1_f16_x4(pntrue, w_ptr);

    for (int d = 0; d < 4; ++d) {
      svfloat16_t w16_row;
      if (d == 0) {
        w16_row = svget4_f16(w16_4rows, 0);
      } else if (d == 1) {
        w16_row = svget4_f16(w16_4rows, 1);
      } else if (d == 2) {
        w16_row = svget4_f16(w16_4rows, 2);
      } else {
        w16_row = svget4_f16(w16_4rows, 3);
      }

      svfloat16_t input_group = svld1_f16(p_root, input_row);
      accumulate_complex_outer_to_za_f16<0, 1>(p_data, p_root, w16_row,
                                               input_group, pimag);

      if constexpr (UnrollTwo) {
        svfloat16_t input_next_group = svld1_f16(p_root, input_next_row);
        accumulate_complex_outer_to_za_f16<2, 3>(p_data, p_root, w16_row,
                                                 input_next_group, pimag);
        input_next_row += stride;
      }

      input_row += stride;
    }

    w_ptr += 4 * offset;
  }
}

/*
  Extract the 16-point FFT results, multiply by twiddle factors, and write the
  intermediate values to scratch in TT layout.

  The first stage uses coefficient-first MOPA, so the root index is held in ZA
  columns. Vertical reads produce the scratch layout needed by the root stage:
  each scratch row contains all 16 outputs for one root point.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void store_16_stride_root_twiddled(
    svbool_t p_data, svcount_t pntrue, const float16_t *tw_f,
    float16_t *scratch, const int32_t root,
    const int64_t delta_buffer) __arm_streaming __arm_inout("za") {
  constexpr int64_t row_stride = 32;
  float16_t *scratch_next = nullptr;
  if constexpr (UnrollTwo) {
    scratch_next = scratch + delta_buffer;
  }

  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t real0_4slices_f32 = svread_ver_za32_f32_vg4(0, slice);
    svfloat32x4_t imag0_4slices_f32 = svread_ver_za32_f32_vg4(1, slice);
    svfloat16x4_t twiddle_4rows =
        svld1_f16_x4(pntrue, tw_f + slice * row_stride);

    svfloat16_t result0 = twiddle_slice<0>(p_data, real0_4slices_f32,
                                           imag0_4slices_f32, twiddle_4rows);
    svfloat16_t result1 = twiddle_slice<1>(p_data, real0_4slices_f32,
                                           imag0_4slices_f32, twiddle_4rows);
    svfloat16_t result2 = twiddle_slice<2>(p_data, real0_4slices_f32,
                                           imag0_4slices_f32, twiddle_4rows);
    svfloat16_t result3 = twiddle_slice<3>(p_data, real0_4slices_f32,
                                           imag0_4slices_f32, twiddle_4rows);

    svst1_f16(p_data, scratch + slice * row_stride, result0);
    svst1_f16(p_data, scratch + (slice + 1) * row_stride, result1);
    svst1_f16(p_data, scratch + (slice + 2) * row_stride, result2);
    svst1_f16(p_data, scratch + (slice + 3) * row_stride, result3);

    if constexpr (UnrollTwo) {
      svfloat32x4_t real1_4slices_f32 = svread_ver_za32_f32_vg4(2, slice);
      svfloat32x4_t imag1_4slices_f32 = svread_ver_za32_f32_vg4(3, slice);

      result0 = twiddle_slice<0>(p_data, real1_4slices_f32, imag1_4slices_f32,
                                 twiddle_4rows);
      result1 = twiddle_slice<1>(p_data, real1_4slices_f32, imag1_4slices_f32,
                                 twiddle_4rows);
      result2 = twiddle_slice<2>(p_data, real1_4slices_f32, imag1_4slices_f32,
                                 twiddle_4rows);
      result3 = twiddle_slice<3>(p_data, real1_4slices_f32, imag1_4slices_f32,
                                 twiddle_4rows);

      svst1_f16(p_data, scratch_next + slice * row_stride, result0);
      svst1_f16(p_data, scratch_next + (slice + 1) * row_stride, result1);
      svst1_f16(p_data, scratch_next + (slice + 2) * row_stride, result2);
      svst1_f16(p_data, scratch_next + (slice + 3) * row_stride, result3);
    }

    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  if (unroll_2) {
    svfloat32x2_t real0_2slices_f32 = svread_ver_za32_f32_vg2(0, slice);
    svfloat32x2_t imag0_2slices_f32 = svread_ver_za32_f32_vg2(1, slice);
    svfloat16_t twiddle0 = svld1_f16(p_data, tw_f + slice * row_stride);
    svfloat16_t twiddle1 = svld1_f16(p_data, tw_f + (slice + 1) * row_stride);

    svfloat16_t row0_f16 = svcvtn_f16_f32_x2(
        svcreate2(svget2(real0_2slices_f32, 0), svget2(imag0_2slices_f32, 0)));
    svfloat16_t row1_f16 = svcvtn_f16_f32_x2(
        svcreate2(svget2(real0_2slices_f32, 1), svget2(imag0_2slices_f32, 1)));
    svfloat16_t result0 = svdup_f16(0.0F);
    svfloat16_t result1 = svdup_f16(0.0F);
    result0 = svcmla_f16_x(p_data, result0, row0_f16, twiddle0, 0);
    result0 = svcmla_f16_x(p_data, result0, row0_f16, twiddle0, 90);
    result1 = svcmla_f16_x(p_data, result1, row1_f16, twiddle1, 0);
    result1 = svcmla_f16_x(p_data, result1, row1_f16, twiddle1, 90);

    svst1_f16(p_data, scratch + slice * row_stride, result0);
    svst1_f16(p_data, scratch + (slice + 1) * row_stride, result1);

    if constexpr (UnrollTwo) {
      svfloat32x2_t real1_2slices_f32 = svread_ver_za32_f32_vg2(2, slice);
      svfloat32x2_t imag1_2slices_f32 = svread_ver_za32_f32_vg2(3, slice);

      row0_f16 = svcvtn_f16_f32_x2(svcreate2(svget2(real1_2slices_f32, 0),
                                             svget2(imag1_2slices_f32, 0)));
      row1_f16 = svcvtn_f16_f32_x2(svcreate2(svget2(real1_2slices_f32, 1),
                                             svget2(imag1_2slices_f32, 1)));
      result0 = svdup_f16(0.0F);
      result1 = svdup_f16(0.0F);
      result0 = svcmla_f16_x(p_data, result0, row0_f16, twiddle0, 0);
      result0 = svcmla_f16_x(p_data, result0, row0_f16, twiddle0, 90);
      result1 = svcmla_f16_x(p_data, result1, row1_f16, twiddle1, 0);
      result1 = svcmla_f16_x(p_data, result1, row1_f16, twiddle1, 90);

      svst1_f16(p_data, scratch_next + slice * row_stride, result0);
      svst1_f16(p_data, scratch_next + (slice + 1) * row_stride, result1);
    }

    slice += 2;
  }

  if (root & 1) {
    svfloat32x2_t real0_2slices_f32 = svread_ver_za32_f32_vg2(0, slice);
    svfloat32x2_t imag0_2slices_f32 = svread_ver_za32_f32_vg2(1, slice);
    svfloat16_t twiddle = svld1_f16(p_data, tw_f + slice * row_stride);
    svfloat16_t row_f16 = svcvtn_f16_f32_x2(
        svcreate2(svget2(real0_2slices_f32, 0), svget2(imag0_2slices_f32, 0)));
    svfloat16_t result = svdup_f16(0.0F);
    result = svcmla_f16_x(p_data, result, row_f16, twiddle, 0);
    result = svcmla_f16_x(p_data, result, row_f16, twiddle, 90);
    svst1_f16(p_data, scratch + slice * row_stride, result);

    if constexpr (UnrollTwo) {
      real0_2slices_f32 = svread_ver_za32_f32_vg2(2, slice);
      imag0_2slices_f32 = svread_ver_za32_f32_vg2(3, slice);
      row_f16 = svcvtn_f16_f32_x2(svcreate2(svget2(real0_2slices_f32, 0),
                                            svget2(imag0_2slices_f32, 0)));
      result = svdup_f16(0.0F);
      result = svcmla_f16_x(p_data, result, row_f16, twiddle, 0);
      result = svcmla_f16_x(p_data, result, row_f16, twiddle, 90);
      svst1_f16(p_data, scratch_next + slice * row_stride, result);
    }
  }
}

/*
  This function calculates one or two root-long FFTs with stride 1 from the
  scratch buffer. The scratch rows are the root points produced by the first
  stage; each vector contains one complete 16-point group for a TT row.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void calculate_root_stride_1(
    svbool_t p_root, svbool_t p_data, svcount_t pntrue, svfloat32_t inactive,
    svbool_t pimag, const float16_t *scratch, const float16_t *w_ptr,
    float16_t *out, const int64_t odist, const int32_t root,
    const int64_t stride,
    const int64_t delta_buffer) __arm_streaming __arm_inout("za") {
  const float16_t *scratch_next = nullptr;
  if constexpr (UnrollTwo) {
    scratch_next = scratch + delta_buffer;
  }

  constexpr int64_t row_stride = 32;
  const int64_t offset = svcnth();

  for (int pt = 0; pt < root; ++pt) {
    svfloat16_t w_root_row = svld1_f16(p_root, w_ptr);
    svfloat16_t input_group = svld1_f16(p_data, scratch + pt * row_stride);

    accumulate_complex_outer_to_za_f16<0, 1>(p_root, p_data, w_root_row,
                                             input_group, pimag);

    if constexpr (UnrollTwo) {
      svfloat16_t input_next_group =
          svld1_f16(p_data, scratch_next + pt * row_stride);
      accumulate_complex_outer_to_za_f16<2, 3>(p_root, p_data, w_root_row,
                                               input_next_group, pimag);
    }

    w_ptr += stride;
  }

  /*
    We only have root rows to extract and write to the output. We minimize tile
    reads by unrolling by 4, then 2, then using an individual read for the last
    row if root is odd.
  */
  float16_t *out_next = nullptr;
  if constexpr (UnrollTwo) {
    out_next = out + odist * 2;
  }

  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t real0_4slices_f32 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t imag0_4slices_f32 = svread_hor_za32_f32_vg4(1, slice);

    svfloat16x4_t results0_to_store = svundef4_f16();
    results0_to_store = svset4_f16(
        results0_to_store, 0,
        get_interleaved_slice<0>(real0_4slices_f32, imag0_4slices_f32));
    results0_to_store = svset4_f16(
        results0_to_store, 1,
        get_interleaved_slice<1>(real0_4slices_f32, imag0_4slices_f32));
    results0_to_store = svset4_f16(
        results0_to_store, 2,
        get_interleaved_slice<2>(real0_4slices_f32, imag0_4slices_f32));
    results0_to_store = svset4_f16(
        results0_to_store, 3,
        get_interleaved_slice<3>(real0_4slices_f32, imag0_4slices_f32));
    svst1_f16_x4(pntrue, out + slice * offset, results0_to_store);

    if constexpr (UnrollTwo) {
      svfloat32x4_t real1_4slices_f32 = svread_hor_za32_f32_vg4(2, slice);
      svfloat32x4_t imag1_4slices_f32 = svread_hor_za32_f32_vg4(3, slice);
      svfloat16x4_t results1_to_store = svundef4_f16();
      results1_to_store = svset4_f16(
          results1_to_store, 0,
          get_interleaved_slice<0>(real1_4slices_f32, imag1_4slices_f32));
      results1_to_store = svset4_f16(
          results1_to_store, 1,
          get_interleaved_slice<1>(real1_4slices_f32, imag1_4slices_f32));
      results1_to_store = svset4_f16(
          results1_to_store, 2,
          get_interleaved_slice<2>(real1_4slices_f32, imag1_4slices_f32));
      results1_to_store = svset4_f16(
          results1_to_store, 3,
          get_interleaved_slice<3>(real1_4slices_f32, imag1_4slices_f32));
      svst1_f16_x4(pntrue, out_next + slice * offset, results1_to_store);
    }

    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  if (unroll_2) {
    svfloat32x2_t real0_2slices_f32 = svread_hor_za32_f32_vg2(0, slice);
    svfloat32x2_t imag0_2slices_f32 = svread_hor_za32_f32_vg2(1, slice);
    svst1_f16(p_data, out + slice * offset,
              svcvtn_f16_f32_x2(svcreate2(svget2(real0_2slices_f32, 0),
                                          svget2(imag0_2slices_f32, 0))));
    svst1_f16(p_data, out + (slice + 1) * offset,
              svcvtn_f16_f32_x2(svcreate2(svget2(real0_2slices_f32, 1),
                                          svget2(imag0_2slices_f32, 1))));

    if constexpr (UnrollTwo) {
      svfloat32x2_t real1_2slices_f32 = svread_hor_za32_f32_vg2(2, slice);
      svfloat32x2_t imag1_2slices_f32 = svread_hor_za32_f32_vg2(3, slice);
      svst1_f16(p_data, out_next + slice * offset,
                svcvtn_f16_f32_x2(svcreate2(svget2(real1_2slices_f32, 0),
                                            svget2(imag1_2slices_f32, 0))));
      svst1_f16(p_data, out_next + (slice + 1) * offset,
                svcvtn_f16_f32_x2(svcreate2(svget2(real1_2slices_f32, 1),
                                            svget2(imag1_2slices_f32, 1))));
    }

    slice += 2;
  }

  if (root & 1) {
    svbool_t pg = svptrue_b32();
    svfloat32_t real0 = svread_hor_za32_f32_m(inactive, pg, 0, slice);
    svfloat32_t imag0 = svread_hor_za32_f32_m(inactive, pg, 1, slice);
    svst1_f16(p_data, out + slice * offset,
              svcvtn_f16_f32_x2(svcreate2(real0, imag0)));

    if constexpr (UnrollTwo) {
      real0 = svread_hor_za32_f32_m(inactive, pg, 2, slice);
      imag0 = svread_hor_za32_f32_m(inactive, pg, 3, slice);
      svst1_f16(p_data, out_next + slice * offset,
                svcvtn_f16_f32_x2(svcreate2(real0, imag0)));
    }
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA static void plfft_16x_jjjn_tt_sme2_impl(
    const complex_half *X, complex_half *Y, int64_t /*istride*/,
    int64_t /*ostride*/, const complex_half *W_n1, const complex_half *W_n2,
    const complex_half *tw, int64_t howmany, int64_t idist, int64_t odist,
    const int64_t n, float16_t *scratch) {
  assert(n >= 32 && n <= 256 && n % 16 == 0);
  // This kernel is specialized for SVL = 512
  assert(svcntw() == 16);

  constexpr int64_t radix = 16;
  // This kernel computes FFTs of length 16x, where x is root.
  const int64_t skip = idist * 2;
  const int64_t delta_buffer = n * 2;
  const int32_t root = n / radix;
  const int64_t stride = root * 2;
  // Split howmany into pairs of TT rows and an odd tail.
  const int64_t even_howmany = howmany & ~1;

  svbool_t p_data = svptrue_b16();
  svbool_t p_root = svwhilelt_b16_s64(0, root * 2);
  const svbool_t pfalse = svpfalse_b();
  const svbool_t pimag = svzip1_b16(pfalse, p_data);
  svcount_t pntrue = svptrue_c16();
  svfloat32_t inactive = svdup_f32(0.0f);

  const auto *X_f = reinterpret_cast<const float16_t *>(X);
  auto *Y_f = reinterpret_cast<float16_t *>(Y);
  const auto *W_16 = reinterpret_cast<const float16_t *>(W_n1);
  const auto *W_root = reinterpret_cast<const float16_t *>(W_n2);
  const auto *tw_f = reinterpret_cast<const float16_t *>(tw);

  /*
    Process two TT rows at a time. Each iteration first computes the length-16
    stride-root stage into ZA, stores twiddled intermediate values into
    scratch, then computes the root-length stride-1 stage into the output.
  */
  for (int64_t r = 0; r < even_howmany; r += 2) {
    const auto *input_row = X_f + r * skip;

    svzero_za();
    calculate_16_stride_root<true>(p_root, p_data, pntrue, pimag, input_row,
                                   W_16, skip, stride);
    store_16_stride_root_twiddled<true>(p_data, pntrue, tw_f, scratch, root,
                                        delta_buffer);

    svzero_za();
    calculate_root_stride_1<true>(p_root, p_data, pntrue, inactive, pimag,
                                  scratch, W_root, Y_f + r * odist * 2, odist,
                                  root, stride, delta_buffer);
  }

  // Tail path for an odd number of TT rows.
  if (howmany & 1) {
    const auto *input_row = X_f + even_howmany * skip;

    svzero_za();
    calculate_16_stride_root<false>(p_root, p_data, pntrue, pimag, input_row,
                                    W_16, skip, stride);
    store_16_stride_root_twiddled<false>(p_data, pntrue, tw_f, scratch, root,
                                         delta_buffer);

    svzero_za();
    calculate_root_stride_1<false>(
        p_root, p_data, pntrue, inactive, pimag, scratch, W_root,
        Y_f + even_howmany * odist * 2, odist, root, stride, delta_buffer);
  }
}

void plfft_16x_jjjn_tt_sme2(const complex_half *X, complex_half *Y,
                            int64_t istride, int64_t ostride,
                            const complex_half *W_n1, const complex_half *W_n2,
                            const complex_half *tw, int64_t howmany,
                            int64_t idist, int64_t odist, const int64_t n) {
  /* This is the largest scratch buffer required:
     2 (rows processed per iteration) * 256 (max FFT length) * 2 (real and
     imag). The buffer holds the twiddled length-16 results before the
     root-long FFT stage. */
  THREAD_LOCAL float16_t scratch[1024];
  plfft_16x_jjjn_tt_sme2_impl(X, Y, istride, ostride, W_n1, W_n2, tw, howmany,
                              idist, odist, n, scratch);
}

} // namespace plfft
