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
PLFFT_ALWAYS_INLINE void
apply_twiddle_and_store_row0(svbool_t p_data, svfloat32x4_t real0_4slices_f32,
                             svfloat32x4_t imag0_4slices_f32,
                             svfloat16_t twiddles_4w_row0,
                             float16_t *&row_tiles_01_out,
                             const int64_t delta_batch_out) __arm_streaming {
  svfloat16_t row0_f16 =
      get_interleaved_slice<Lane>(real0_4slices_f32, imag0_4slices_f32);
  svfloat16_t result0 = svdup_f16(0.0F);
  result0 = svcmla_lane_f16(result0, row0_f16, twiddles_4w_row0, Lane, 0);
  result0 = svcmla_lane_f16(result0, row0_f16, twiddles_4w_row0, Lane, 90);
  svst1_f16(p_data, row_tiles_01_out, result0);
  row_tiles_01_out += delta_batch_out;
}

template<int Lane>
PLFFT_ALWAYS_INLINE void
apply_twiddle_and_store_row1(svbool_t p_data, svfloat32x4_t real1_4slices_f32,
                             svfloat32x4_t imag1_4slices_f32,
                             svfloat16_t twiddles_4w_row1,
                             float16_t *&row_tiles_23_out,
                             const int64_t delta_batch_out) __arm_streaming {
  svfloat16_t row1_f16 =
      get_interleaved_slice<Lane>(real1_4slices_f32, imag1_4slices_f32);
  svfloat16_t result1 = svdup_f16(0.0F);
  result1 = svcmla_lane_f16(result1, row1_f16, twiddles_4w_row1, Lane, 0);
  result1 = svcmla_lane_f16(result1, row1_f16, twiddles_4w_row1, Lane, 90);
  svst1_f16(p_data, row_tiles_23_out, result1);
  row_tiles_23_out += delta_batch_out;
}

template<int Lane>
PLFFT_ALWAYS_INLINE void apply_twiddle_and_store_2rows(
    svbool_t p_data, svfloat32x4_t real0_4slices_f32,
    svfloat32x4_t imag0_4slices_f32, svfloat32x4_t real1_4slices_f32,
    svfloat32x4_t imag1_4slices_f32, svfloat16_t twiddles_4w_row0,
    svfloat16_t twiddles_4w_row1, float16_t *&row_tiles_01_out,
    float16_t *&row_tiles_23_out,
    const int64_t delta_batch_out) __arm_streaming {
  apply_twiddle_and_store_row0<Lane>(p_data, real0_4slices_f32,
                                     imag0_4slices_f32, twiddles_4w_row0,
                                     row_tiles_01_out, delta_batch_out);
  apply_twiddle_and_store_row1<Lane>(p_data, real1_4slices_f32,
                                     imag1_4slices_f32, twiddles_4w_row1,
                                     row_tiles_23_out, delta_batch_out);
}

/*
  Given a batch of 16, this function calculates the 16-long FFTs of stride =
  root. It can calculate either 1 or 2 batches at the same time, depending on
  the template parameter. Tiles za0,za1 are used for the first batch while
  za2,za3 are used for the second batch when UnrollTwo = true.

  The calculated FFTs are extracted from the tiles, multiplied by twiddle
  factors, and written into the temporary buffer with ostride = 16.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void calculate_16_stride_root_and_twiddle(
    svbool_t p_row, svbool_t p_data, svcount_t pntrue, svbool_t pimag,
    const float16_t *row_tiles_01_in, float16_t *row_tiles_01_out,
    const float16_t *w_ptr, const float16_t *tw_ri_pts, const int64_t skip_in,
    const int64_t skip_out, const int64_t delta_batch_in,
    const int64_t delta_batch_out) __arm_streaming __arm_inout("za") {

  const int64_t offset = svcnth();

  const float16_t *row_tiles_23_in = nullptr;
  float16_t *row_tiles_23_out = nullptr;
  if constexpr (UnrollTwo) {
    row_tiles_23_in = row_tiles_01_in + skip_in;
    row_tiles_23_out = row_tiles_01_out + skip_out;
  }

  /* Calculate a batch of 16 FFTs of length 16 and stride = root. When
     UnrollTwo = true, calculate 2 batches and use all four ZA tiles. */
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
      svfloat16_t input_set0 = svld1_f16(p_data, row_tiles_01_in);
      accumulate_complex_outer_to_za_f16<0, 1>(p_row, p_data, w16_row,
                                               input_set0, pimag);
      row_tiles_01_in += delta_batch_in;

      if constexpr (UnrollTwo) {
        svfloat16_t input_set1 = svld1_f16(p_data, row_tiles_23_in);
        accumulate_complex_outer_to_za_f16<2, 3>(p_row, p_data, w16_row,
                                                 input_set1, pimag);
        row_tiles_23_in += delta_batch_in;
      }
    }
    w_ptr += 4 * offset;
  }

  const auto *tw_row0 = tw_ri_pts;
  const auto *tw_row1 = tw_ri_pts + offset;

  // Extract the 16-point FFT results and multiply by the twiddle factors.
  for (int slice = 0; slice < 16; slice += 4) {
    svfloat16_t twiddles_4w_row0 = svld1rq_f16(p_row, tw_row0);
    svfloat32x4_t real0_4slices_f32 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t imag0_4slices_f32 = svread_hor_za32_f32_vg4(1, slice);

    if constexpr (UnrollTwo) {
      svfloat16_t twiddles_4w_row1 = svld1rq_f16(p_row, tw_row1);
      svfloat32x4_t real1_4slices_f32 = svread_hor_za32_f32_vg4(2, slice);
      svfloat32x4_t imag1_4slices_f32 = svread_hor_za32_f32_vg4(3, slice);

      apply_twiddle_and_store_2rows<0>(
          p_data, real0_4slices_f32, imag0_4slices_f32, real1_4slices_f32,
          imag1_4slices_f32, twiddles_4w_row0, twiddles_4w_row1,
          row_tiles_01_out, row_tiles_23_out, delta_batch_out);
      apply_twiddle_and_store_2rows<1>(
          p_data, real0_4slices_f32, imag0_4slices_f32, real1_4slices_f32,
          imag1_4slices_f32, twiddles_4w_row0, twiddles_4w_row1,
          row_tiles_01_out, row_tiles_23_out, delta_batch_out);
      apply_twiddle_and_store_2rows<2>(
          p_data, real0_4slices_f32, imag0_4slices_f32, real1_4slices_f32,
          imag1_4slices_f32, twiddles_4w_row0, twiddles_4w_row1,
          row_tiles_01_out, row_tiles_23_out, delta_batch_out);
      apply_twiddle_and_store_2rows<3>(
          p_data, real0_4slices_f32, imag0_4slices_f32, real1_4slices_f32,
          imag1_4slices_f32, twiddles_4w_row0, twiddles_4w_row1,
          row_tiles_01_out, row_tiles_23_out, delta_batch_out);
    } else {
      apply_twiddle_and_store_row0<0>(p_data, real0_4slices_f32,
                                      imag0_4slices_f32, twiddles_4w_row0,
                                      row_tiles_01_out, delta_batch_out);
      apply_twiddle_and_store_row0<1>(p_data, real0_4slices_f32,
                                      imag0_4slices_f32, twiddles_4w_row0,
                                      row_tiles_01_out, delta_batch_out);
      apply_twiddle_and_store_row0<2>(p_data, real0_4slices_f32,
                                      imag0_4slices_f32, twiddles_4w_row0,
                                      row_tiles_01_out, delta_batch_out);
      apply_twiddle_and_store_row0<3>(p_data, real0_4slices_f32,
                                      imag0_4slices_f32, twiddles_4w_row0,
                                      row_tiles_01_out, delta_batch_out);
    }

    tw_row0 += 8;
    tw_row1 += 8;
  }
}

PLFFT_ALWAYS_INLINE void save_transform(const svbool_t pg, const int64_t offset,
                                        svfloat32_t real, svfloat32_t imag,
                                        float16_t *out) __arm_streaming {
  svfloat32x2_t to_store = svcreate2(real, imag);
  svfloat16_t to_store_f16 = svcvtn_f16_f32_x2(to_store);
  svst1_f16(pg, out + offset, to_store_f16);
}

/*
  This function calculates a batch of 16 FFTs, all root-long with stride 1. In
  each batch of 16, there are 16 transforms to calculate, because each
  individual FFT is long 16*root. For this reason, we always use za0,za1 and
  za2,za3 to calculate two batches of root-long FFTs.
*/
PLFFT_ALWAYS_INLINE void calculate_root_stride_1(
    svbool_t p_data, svbool_t p_w, svbool_t pimag,
    const float16_t *row_tiles_01_in, float16_t *row_tiles_01_out,
    const float16_t *w_ptr, const int64_t root, const int64_t group,
    const int64_t skip_in, const int64_t delta_batch_in, const int64_t skip_out,
    const int64_t delta_batch_out) __arm_streaming __arm_inout("za") {

  svbool_t pg = svptrue_b32();
  svfloat32_t inactive = svdup_f32(0.0f);

  const float16_t *row_tiles_23_in = row_tiles_01_in + delta_batch_in;

  for (int pt = 0; pt < root; pt++) {
    svfloat16_t w_root_row = svld1_f16(p_w, w_ptr);

    svfloat16_t input_group0 = svld1_f16(p_data, row_tiles_01_in);
    svfloat16_t input_group1 = svld1_f16(p_data, row_tiles_23_in);
    accumulate_complex_outer_to_za_f16<0, 1>(p_w, p_data, w_root_row,
                                             input_group0, pimag);
    accumulate_complex_outer_to_za_f16<2, 3>(p_w, p_data, w_root_row,
                                             input_group1, pimag);

    row_tiles_01_in += skip_in;
    row_tiles_23_in += skip_in;
    w_ptr += root * 2;
  }

  /*
    We only have root rows to extract and to write in the output in a
    transposed way. We minimize tile reads by unrolling by 4, then 2, then
    using an individual read for the last row if root is odd.
  */
  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t res0 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t res1 = svread_hor_za32_f32_vg4(1, slice);
    svfloat32x4_t res2 = svread_hor_za32_f32_vg4(2, slice);
    svfloat32x4_t res3 = svread_hor_za32_f32_vg4(3, slice);

    save_transform(p_data, (16 * slice + group) * skip_out, svget4(res0, 0),
                   svget4(res1, 0), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 1) + group) * skip_out,
                   svget4(res0, 1), svget4(res1, 1), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 2) + group) * skip_out,
                   svget4(res0, 2), svget4(res1, 2), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 3) + group) * skip_out,
                   svget4(res0, 3), svget4(res1, 3), row_tiles_01_out);

    save_transform(p_data, (16 * slice + (group + 1)) * skip_out,
                   svget4(res2, 0), svget4(res3, 0), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 1) + (group + 1)) * skip_out,
                   svget4(res2, 1), svget4(res3, 1), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 2) + (group + 1)) * skip_out,
                   svget4(res2, 2), svget4(res3, 2), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 3) + (group + 1)) * skip_out,
                   svget4(res2, 3), svget4(res3, 3), row_tiles_01_out);

    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  if (unroll_2) {
    svfloat32x2_t res0 = svread_hor_za32_f32_vg2(0, slice);
    svfloat32x2_t res1 = svread_hor_za32_f32_vg2(1, slice);
    svfloat32x2_t res2 = svread_hor_za32_f32_vg2(2, slice);
    svfloat32x2_t res3 = svread_hor_za32_f32_vg2(3, slice);

    save_transform(p_data, (16 * slice + group) * skip_out, svget2(res0, 0),
                   svget2(res1, 0), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 1) + group) * skip_out,
                   svget2(res0, 1), svget2(res1, 1), row_tiles_01_out);
    save_transform(p_data, (16 * slice + (group + 1)) * skip_out,
                   svget2(res2, 0), svget2(res3, 0), row_tiles_01_out);
    save_transform(p_data, (16 * (slice + 1) + (group + 1)) * skip_out,
                   svget2(res2, 1), svget2(res3, 1), row_tiles_01_out);

    slice += 2;
  }

  const int32_t last_row = root & 1;
  if (last_row) {
    svfloat32_t res0 = svread_hor_za32_f32_m(inactive, pg, 0, slice);
    svfloat32_t res1 = svread_hor_za32_f32_m(inactive, pg, 1, slice);
    svfloat32_t res2 = svread_hor_za32_f32_m(inactive, pg, 2, slice);
    svfloat32_t res3 = svread_hor_za32_f32_m(inactive, pg, 3, slice);

    save_transform(p_data, (16 * slice + group) * skip_out, res0, res1,
                   row_tiles_01_out);
    save_transform(p_data, (16 * slice + (group + 1)) * skip_out, res2, res3,
                   row_tiles_01_out);
  }
}

PLFFT_ALWAYS_INLINE void
calculate_group(const int n, const float16_t *X, float16_t *Y, int64_t istride,
                int64_t ostride, const float16_t *W_16, const float16_t *W_root,
                const float16_t *tw, const svbool_t p_row,
                const svbool_t p_data, const svcount_t pntrue,
                const svbool_t pimag,
                float16_t *buffer) __arm_streaming __arm_inout("za") {
  constexpr int32_t radix = 16;
  // This kernel computes FFTs of length 16x, where x is root.
  const int64_t root = n / radix;
  // Split root into the part unrolled by two and the odd remainder.
  const int64_t even_root = root & ~1;
  const int64_t odd_root = root & 1;
  // Transform strides on complex values into strides on real values.
  const int64_t skip_in = istride * 2;
  const int64_t skip_out = ostride * 2;
  // Strides when considering transforms with stride = root.
  const int64_t delta_batch_in = skip_in * root;
  const int64_t delta_batch_out = skip_out * root;
  // Internal buffer strides.
  const int64_t skip_out_buffer = 16 * 2;
  const int64_t delta_batch_out_buffer = skip_out_buffer * root;

  const float16_t *input_point = nullptr;
  float16_t *output_point = nullptr;
  const float16_t *tw_ri_pts = nullptr;

  // FFTs of length 16 and stride = root, then multiply by twiddles.
  for (int row = 0; row < even_root; row += 2) {
    svzero_za();
    tw_ri_pts = tw + row * 32;
    input_point = X + row * skip_in;
    output_point = buffer + row * skip_out_buffer;
    calculate_16_stride_root_and_twiddle<true>(
        p_row, p_data, pntrue, pimag, input_point, output_point, W_16,
        tw_ri_pts, skip_in, skip_out_buffer, delta_batch_in,
        delta_batch_out_buffer);
  }
  if (odd_root) {
    svzero_za();
    input_point = X + even_root * skip_in;
    output_point = buffer + even_root * skip_out_buffer;
    tw_ri_pts = tw + even_root * 32;
    calculate_16_stride_root_and_twiddle<false>(
        p_row, p_data, pntrue, pimag, input_point, output_point, W_16,
        tw_ri_pts, skip_in, skip_out_buffer, delta_batch_in,
        delta_batch_out_buffer);
  }

  svbool_t p_w = svwhilelt_b16_s64(0, root * 2);
  // FFTs of length root and stride = 1, transposed into the output array.
  for (int group = 0; group < 16; group += 2) {
    input_point = buffer + group * delta_batch_out_buffer;
    output_point = Y;
    svzero_za();
    calculate_root_stride_1(p_data, p_w, pimag, input_point, output_point,
                            W_root, root, group, skip_out_buffer,
                            delta_batch_out_buffer, skip_out, delta_batch_out);
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA static void plfft_16x_jjjn_uu_sme2_impl(
    const complex_half *X, complex_half *Y, int64_t istride, int64_t ostride,
    const complex_half *W_n1, const complex_half *W_n2, const complex_half *tw,
    int64_t howmany, int64_t /*idist*/, int64_t /*odist*/, const int64_t n,
    float16_t *buffer) {

  assert(n >= 32 && n <= 256 && n % 16 == 0);
  // This kernel is specialized for SVL = 512
  assert(svcntw() == 16);

  svbool_t p_row = svptrue_b16();
  svcount_t pntrue = svptrue_c16();
  const svbool_t pfalse = svpfalse_b();
  const svbool_t pimag = svzip1_b16(pfalse, p_row);

  const auto *W_16 = reinterpret_cast<const float16_t *>(W_n1);
  const auto *W_root = reinterpret_cast<const float16_t *>(W_n2);
  for (int col_group = 0; col_group < howmany; col_group += 16) {
    const svbool_t p_data = svwhilelt_b16_s64(0, 2 * (howmany - col_group));
    const auto *tw_ri_f = reinterpret_cast<const float16_t *>(tw);
    const auto *x_f_in = reinterpret_cast<const float16_t *>(&X[col_group]);
    auto *y_f_out = reinterpret_cast<float16_t *>(&Y[col_group]);

    calculate_group(n, x_f_in, y_f_out, istride, ostride, W_16, W_root, tw_ri_f,
                    p_row, p_data, pntrue, pimag, buffer);
  }
}

void plfft_16x_jjjn_uu_sme2(const complex_half *X, complex_half *Y,
                            int64_t istride, int64_t ostride,
                            const complex_half *W_n1, const complex_half *W_n2,
                            const complex_half *tw, int64_t howmany,
                            int64_t idist, int64_t odist, const int64_t n) {
  /* This is the largest buffer we may require:
     256 (rows - max FFT length) * 16 (cols - batch size) * 2 (real and imag).
     The buffer holds the intermediate 16-long FFT results after twiddling so
     that the root-long FFT can write transposed output safely. */
  THREAD_LOCAL float16_t buffer[8192];
  plfft_16x_jjjn_uu_sme2_impl(X, Y, istride, ostride, W_n1, W_n2, tw, howmany,
                              idist, odist, n, buffer);
}

} // namespace plfft
