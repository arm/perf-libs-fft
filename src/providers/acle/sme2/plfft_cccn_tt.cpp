/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft_assert.hpp"
#include "plfft_util.hpp"
#include "utils_sme2_f32.hpp"

#include <arm_sme.h>

namespace plfft {

PLFFT_ALWAYS_INLINE void
store_twiddled_split_complex_vnum(const svbool_t pg, const svfloat32_t value_re,
                                  const svfloat32_t value_im,
                                  const svfloat32x2_t tw_re_im, float32_t *out,
                                  const int64_t complex_vnum) __arm_streaming {
  svfloat32_t out_re = svmul_f32_x(pg, value_re, svget2(tw_re_im, 0));
  svfloat32_t out_im = svmul_f32_x(pg, value_re, svget2(tw_re_im, 1));
  out_re = svmls_f32_m(pg, out_re, value_im, svget2(tw_re_im, 1));
  out_im = svmla_f32_m(pg, out_im, value_im, svget2(tw_re_im, 0));
  svst1_vnum_f32(pg, out, 2 * complex_vnum, out_re);
  svst1_vnum_f32(pg, out, 2 * complex_vnum + 1, out_im);
}

PLFFT_ALWAYS_INLINE svfloat32x2_t
load_split_complex_vnum(const svbool_t pg, const float32_t *in,
                        const int64_t complex_vnum) __arm_streaming {
  return svcreate2(svld1_vnum_f32(pg, in, 2 * complex_vnum),
                   svld1_vnum_f32(pg, in, 2 * complex_vnum + 1));
}

/*
  This stage calculates one or two length-16 FFTs with stride = root.
  The input rows are howmany transforms apart: za0/za1 hold the first
  transform row, and za2/za3 hold the next row when UnrollTwo is true.

  The DFT matrix is length 16. Coefficients 0 and 8 are real-only, so they are
  handled explicitly, with the two complex ranges on either side accumulated by
  the loops.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void calculate_16_stride_root(
    const svbool_t pg, const svbool_t px, const float32_t *input_row,
    const float32_t *dft_ptr, const int64_t skip,
    const int32_t stride) __arm_streaming __arm_inout("za") {
  const float32_t *input_next_row = nullptr;
  if constexpr (UnrollTwo) {
    input_next_row = input_row + skip;
  }

  svfloat32x2_t dft_re_im = svld2_f32(pg, dft_ptr);
  svfloat32x2_t input_re_im = svld2_f32(px, input_row);

  svmopa_za32_f32_m(0, px, pg, svget2(input_re_im, 0), svget2(dft_re_im, 0));
  svmopa_za32_f32_m(1, px, pg, svget2(input_re_im, 1), svget2(dft_re_im, 0));

  if constexpr (UnrollTwo) {
    svfloat32x2_t input_next_row_re_im = svld2_f32(px, input_next_row);
    svmopa_za32_f32_m(2, px, pg, svget2(input_next_row_re_im, 0),
                      svget2(dft_re_im, 0));
    svmopa_za32_f32_m(3, px, pg, svget2(input_next_row_re_im, 1),
                      svget2(dft_re_im, 0));
  }

  input_row += stride;
  dft_ptr += 32;

  for (int i = 0; i < 7; i++) {
    dft_re_im = svld2_f32(pg, dft_ptr);
    input_re_im = svld2_f32(px, input_row);

    accumulate_complex_outer_to_za<0, 1>(px, pg, input_re_im, dft_re_im);
    if constexpr (UnrollTwo) {
      svfloat32x2_t input_next_row_re_im = svld2_f32(px, input_row + skip);
      accumulate_complex_outer_to_za<2, 3>(px, pg, input_next_row_re_im,
                                           dft_re_im);
    }

    input_row += stride;
    dft_ptr += 32;
  }

  dft_re_im = svld2_f32(pg, dft_ptr);
  input_re_im = svld2_f32(px, input_row);
  svmopa_za32_f32_m(0, px, pg, svget2(input_re_im, 0), svget2(dft_re_im, 0));
  svmopa_za32_f32_m(1, px, pg, svget2(input_re_im, 1), svget2(dft_re_im, 0));

  if constexpr (UnrollTwo) {
    svfloat32x2_t input_next_row_re_im = svld2_f32(px, input_row + skip);
    svmopa_za32_f32_m(2, px, pg, svget2(input_next_row_re_im, 0),
                      svget2(dft_re_im, 0));
    svmopa_za32_f32_m(3, px, pg, svget2(input_next_row_re_im, 1),
                      svget2(dft_re_im, 0));
  }

  input_row += stride;
  dft_ptr += 32;

  for (int i = 0; i < 7; i++) {
    dft_re_im = svld2_f32(pg, dft_ptr);
    input_re_im = svld2_f32(px, input_row);

    accumulate_complex_outer_to_za<0, 1>(px, pg, input_re_im, dft_re_im);
    if constexpr (UnrollTwo) {
      svfloat32x2_t input_next_row_re_im = svld2_f32(px, input_row + skip);
      accumulate_complex_outer_to_za<2, 3>(px, pg, input_next_row_re_im,
                                           dft_re_im);
    }

    input_row += stride;
    dft_ptr += 32;
  }
}

/*
  Extract the length-16 results from ZA, multiply by the TT twiddle layout, and
  write the intermediate values to the scratch buffer in split-complex form.

  The row index in ZA is the point index inside the root-length transform that
  follows. Each complex_vnum in scratch stores one row as all real lanes
  followed by all imaginary lanes; the second transform row is delta_buffer
  floats after the first when UnrollTwo is true.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void store_16_stride_root_twiddled(
    const svbool_t pg, const svfloat32_t inactive, const float32_t *tw_f,
    float32_t *scratch, const int32_t root,
    const int64_t delta_buffer) __arm_streaming __arm_inout("za") {
  float32_t *scratch_row = scratch;
  float32_t *scratch_next_row = nullptr;
  if constexpr (UnrollTwo) {
    scratch_next_row = scratch_row + delta_buffer;
  }

  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t za_slice_re_first_row = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t za_slice_im_first_row = svread_hor_za32_f32_vg4(1, slice);
    svfloat32x4_t za_slice_re_second_row;
    svfloat32x4_t za_slice_im_second_row;
    if constexpr (UnrollTwo) {
      za_slice_re_second_row = svread_hor_za32_f32_vg4(2, slice);
      za_slice_im_second_row = svread_hor_za32_f32_vg4(3, slice);
    }

    const int32_t row0 = slice;
    const int32_t row1 = slice + 1;
    const int32_t row2 = slice + 2;
    const int32_t row3 = slice + 3;
    svfloat32x2_t tw0 = svld2_vnum_f32(pg, tw_f, 2 * row0);
    svfloat32x2_t tw1 = svld2_vnum_f32(pg, tw_f, 2 * row1);
    svfloat32x2_t tw2 = svld2_vnum_f32(pg, tw_f, 2 * row2);
    svfloat32x2_t tw3 = svld2_vnum_f32(pg, tw_f, 2 * row3);

    store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_first_row, 0),
                                      svget4(za_slice_im_first_row, 0), tw0,
                                      scratch_row, row0);
    store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_first_row, 1),
                                      svget4(za_slice_im_first_row, 1), tw1,
                                      scratch_row, row1);
    store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_first_row, 2),
                                      svget4(za_slice_im_first_row, 2), tw2,
                                      scratch_row, row2);
    store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_first_row, 3),
                                      svget4(za_slice_im_first_row, 3), tw3,
                                      scratch_row, row3);

    if constexpr (UnrollTwo) {
      store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_second_row, 0),
                                        svget4(za_slice_im_second_row, 0), tw0,
                                        scratch_next_row, row0);
      store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_second_row, 1),
                                        svget4(za_slice_im_second_row, 1), tw1,
                                        scratch_next_row, row1);
      store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_second_row, 2),
                                        svget4(za_slice_im_second_row, 2), tw2,
                                        scratch_next_row, row2);
      store_twiddled_split_complex_vnum(pg, svget4(za_slice_re_second_row, 3),
                                        svget4(za_slice_im_second_row, 3), tw3,
                                        scratch_next_row, row3);
    }

    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  for (int ur = 0; ur < unroll_2; ur++) {
    svfloat32x2_t za_slice_re_first_row = svread_hor_za32_f32_vg2(0, slice);
    svfloat32x2_t za_slice_im_first_row = svread_hor_za32_f32_vg2(1, slice);
    svfloat32x2_t za_slice_re_second_row;
    svfloat32x2_t za_slice_im_second_row;
    if constexpr (UnrollTwo) {
      za_slice_re_second_row = svread_hor_za32_f32_vg2(2, slice);
      za_slice_im_second_row = svread_hor_za32_f32_vg2(3, slice);
    }

    const int32_t row0 = slice;
    const int32_t row1 = slice + 1;
    svfloat32x2_t tw0 = svld2_vnum_f32(pg, tw_f, 2 * row0);
    svfloat32x2_t tw1 = svld2_vnum_f32(pg, tw_f, 2 * row1);

    store_twiddled_split_complex_vnum(pg, svget2(za_slice_re_first_row, 0),
                                      svget2(za_slice_im_first_row, 0), tw0,
                                      scratch_row, row0);
    store_twiddled_split_complex_vnum(pg, svget2(za_slice_re_first_row, 1),
                                      svget2(za_slice_im_first_row, 1), tw1,
                                      scratch_row, row1);

    if constexpr (UnrollTwo) {
      store_twiddled_split_complex_vnum(pg, svget2(za_slice_re_second_row, 0),
                                        svget2(za_slice_im_second_row, 0), tw0,
                                        scratch_next_row, row0);
      store_twiddled_split_complex_vnum(pg, svget2(za_slice_re_second_row, 1),
                                        svget2(za_slice_im_second_row, 1), tw1,
                                        scratch_next_row, row1);
    }

    slice += 2;
  }

  if (root & 1) {
    svfloat32_t re_first_row = svread_hor_za32_f32_m(inactive, pg, 0, slice);
    svfloat32_t im_first_row = svread_hor_za32_f32_m(inactive, pg, 1, slice);
    svfloat32x2_t tw0 = svld2_vnum_f32(pg, tw_f, 2 * slice);

    store_twiddled_split_complex_vnum(pg, re_first_row, im_first_row, tw0,
                                      scratch_row, slice);

    if constexpr (UnrollTwo) {
      svfloat32_t re_second_row = svread_hor_za32_f32_m(inactive, pg, 2, slice);
      svfloat32_t im_second_row = svread_hor_za32_f32_m(inactive, pg, 3, slice);
      store_twiddled_split_complex_vnum(pg, re_second_row, im_second_row, tw0,
                                        scratch_next_row, slice);
    }
  }
}

/*
  This stage calculates two root-length FFTs from the split-complex scratch
  buffer and writes them to the output in interleaved-complex form. The two rows
  correspond to adjacent howmany transforms, so the second output row is odist
  complex values after the first.
*/
PLFFT_ALWAYS_INLINE void calculate_root_stride_1_pair(
    const svbool_t pg, const svbool_t px, const svfloat32_t inactive,
    const float32_t *w_ptr, const float32_t *scratch, float32_t *out,
    const int64_t odist, const int32_t root, const int32_t stride,
    const int64_t delta_buffer) __arm_streaming __arm_inout("za") {
  const float32_t *scratch_dft_row = scratch;
  const float32_t *scratch_next_dft_row = scratch_dft_row + delta_buffer;

  svfloat32x2_t coeff = svld2_f32(px, w_ptr);
  svfloat32x2_t scr_row = load_split_complex_vnum(pg, scratch_dft_row, 0);
  svfloat32x2_t scr_next_row =
      load_split_complex_vnum(pg, scratch_next_dft_row, 0);

  svmopa_za32_f32_m(0, pg, pg, svget2(coeff, 0), svget2(scr_row, 0));
  svmopa_za32_f32_m(1, pg, pg, svget2(coeff, 0), svget2(scr_row, 1));
  svmopa_za32_f32_m(2, pg, pg, svget2(coeff, 0), svget2(scr_next_row, 0));
  svmopa_za32_f32_m(3, pg, pg, svget2(coeff, 0), svget2(scr_next_row, 1));

  w_ptr += stride;

  for (int pt = 1; pt < root; ++pt) {
    coeff = svld2_f32(px, w_ptr);
    scr_row = load_split_complex_vnum(pg, scratch_dft_row, pt);
    scr_next_row = load_split_complex_vnum(pg, scratch_next_dft_row, pt);

    accumulate_complex_outer_to_za<0, 1>(pg, pg, coeff, scr_row);
    accumulate_complex_outer_to_za<2, 3>(pg, pg, coeff, scr_next_row);

    w_ptr += stride;
  }

  float32_t *out_next_row = out + odist * 2;
  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t res0 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t res1 = svread_hor_za32_f32_vg4(1, slice);
    svfloat32x4_t res2 = svread_hor_za32_f32_vg4(2, slice);
    svfloat32x4_t res3 = svread_hor_za32_f32_vg4(3, slice);

    svst2_vnum_f32(pg, out, 2 * slice,
                   svcreate2(svget4(res0, 0), svget4(res1, 0)));
    svst2_vnum_f32(pg, out_next_row, 2 * slice,
                   svcreate2(svget4(res2, 0), svget4(res3, 0)));
    svst2_vnum_f32(pg, out, 2 * (slice + 1),
                   svcreate2(svget4(res0, 1), svget4(res1, 1)));
    svst2_vnum_f32(pg, out_next_row, 2 * (slice + 1),
                   svcreate2(svget4(res2, 1), svget4(res3, 1)));
    svst2_vnum_f32(pg, out, 2 * (slice + 2),
                   svcreate2(svget4(res0, 2), svget4(res1, 2)));
    svst2_vnum_f32(pg, out_next_row, 2 * (slice + 2),
                   svcreate2(svget4(res2, 2), svget4(res3, 2)));
    svst2_vnum_f32(pg, out, 2 * (slice + 3),
                   svcreate2(svget4(res0, 3), svget4(res1, 3)));
    svst2_vnum_f32(pg, out_next_row, 2 * (slice + 3),
                   svcreate2(svget4(res2, 3), svget4(res3, 3)));
    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  for (int ur = 0; ur < unroll_2; ur++) {
    svfloat32x2_t res0 = svread_hor_za32_f32_vg2(0, slice);
    svfloat32x2_t res1 = svread_hor_za32_f32_vg2(1, slice);
    svfloat32x2_t res2 = svread_hor_za32_f32_vg2(2, slice);
    svfloat32x2_t res3 = svread_hor_za32_f32_vg2(3, slice);

    svst2_vnum_f32(pg, out, 2 * slice,
                   svcreate2(svget2(res0, 0), svget2(res1, 0)));
    svst2_vnum_f32(pg, out_next_row, 2 * slice,
                   svcreate2(svget2(res2, 0), svget2(res3, 0)));
    svst2_vnum_f32(pg, out, 2 * (slice + 1),
                   svcreate2(svget2(res0, 1), svget2(res1, 1)));
    svst2_vnum_f32(pg, out_next_row, 2 * (slice + 1),
                   svcreate2(svget2(res2, 1), svget2(res3, 1)));

    slice += 2;
  }

  if (root & 1) {
    svfloat32_t res0 = svread_hor_za32_f32_m(inactive, pg, 0, slice);
    svfloat32_t res1 = svread_hor_za32_f32_m(inactive, pg, 1, slice);
    svfloat32_t res2 = svread_hor_za32_f32_m(inactive, pg, 2, slice);
    svfloat32_t res3 = svread_hor_za32_f32_m(inactive, pg, 3, slice);
    svst2_vnum_f32(pg, out, 2 * slice, svcreate2(res0, res1));
    svst2_vnum_f32(pg, out_next_row, 2 * slice, svcreate2(res2, res3));
  }
}

/*
  Same root-length stage as calculate_root_stride_1_pair, but for the final odd
  howmany row. Only za0/za1 are populated and written.
*/
PLFFT_ALWAYS_INLINE void calculate_root_stride_1_single(
    const svbool_t pg, const svbool_t px, const svfloat32_t inactive,
    const float32_t *w_ptr, const float32_t *scratch, float32_t *out,
    const int32_t root,
    const int32_t stride) __arm_streaming __arm_inout("za") {
  const float32_t *scratch_dft_row = scratch;

  svfloat32x2_t coeff = svld2_f32(px, w_ptr);
  svfloat32x2_t scr_row = load_split_complex_vnum(pg, scratch_dft_row, 0);

  svmopa_za32_f32_m(0, pg, pg, svget2(coeff, 0), svget2(scr_row, 0));
  svmopa_za32_f32_m(1, pg, pg, svget2(coeff, 0), svget2(scr_row, 1));

  w_ptr += stride;

  for (int pt = 1; pt < root; ++pt) {
    coeff = svld2_f32(px, w_ptr);
    scr_row = load_split_complex_vnum(pg, scratch_dft_row, pt);

    accumulate_complex_outer_to_za<0, 1>(pg, pg, coeff, scr_row);

    w_ptr += stride;
  }

  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t res0 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t res1 = svread_hor_za32_f32_vg4(1, slice);

    svst2_vnum_f32(pg, out, 2 * slice,
                   svcreate2(svget4(res0, 0), svget4(res1, 0)));
    svst2_vnum_f32(pg, out, 2 * (slice + 1),
                   svcreate2(svget4(res0, 1), svget4(res1, 1)));
    svst2_vnum_f32(pg, out, 2 * (slice + 2),
                   svcreate2(svget4(res0, 2), svget4(res1, 2)));
    svst2_vnum_f32(pg, out, 2 * (slice + 3),
                   svcreate2(svget4(res0, 3), svget4(res1, 3)));
    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  for (int ur = 0; ur < unroll_2; ur++) {
    svfloat32x2_t res0 = svread_hor_za32_f32_vg2(0, slice);
    svfloat32x2_t res1 = svread_hor_za32_f32_vg2(1, slice);

    svst2_vnum_f32(pg, out, 2 * slice,
                   svcreate2(svget2(res0, 0), svget2(res1, 0)));
    svst2_vnum_f32(pg, out, 2 * (slice + 1),
                   svcreate2(svget2(res0, 1), svget2(res1, 1)));

    slice += 2;
  }

  if (root & 1) {
    svfloat32_t res0 = svread_hor_za32_f32_m(inactive, pg, 0, slice);
    svfloat32_t res1 = svread_hor_za32_f32_m(inactive, pg, 1, slice);
    svst2_vnum_f32(pg, out, 2 * slice, svcreate2(res0, res1));
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA static void plfft_16x_cccn_tt_sme2_impl(
    const std::complex<float> *X, std::complex<float> *Y, int64_t /*istride*/,
    int64_t /*ostride*/, const std::complex<float> *W_n1,
    const std::complex<float> *W_n2, const std::complex<float> *tw,
    int64_t howmany, int64_t idist, int64_t odist, const int64_t n,
    float32_t *scratch) {
  // This kernel is currently specialized for a 512 bits Scalar Vector Length
  // which corresponds to 16 float32 elements per vector.
  assert(svcntw() == 16);

  constexpr int64_t radix = 16;
  const int64_t skip = idist * 2;
  const int64_t delta_buffer = n * 2;

  // This is valid as n <= 256
  const int32_t root = n / radix;
  const int32_t stride = root * 2;
  const auto even_howmany = howmany & ~1;

  svbool_t pg = svptrue_b32();
  svbool_t px = svwhilelt_b32_s64(0, root);
  svfloat32_t inactive = svdup_f32(0.0f);

  const auto *X_f = reinterpret_cast<const float32_t *>(X);
  auto *Y_f = reinterpret_cast<float32_t *>(Y);
  const auto *W_16 = reinterpret_cast<const float32_t *>(W_n1);
  const auto *W_root = reinterpret_cast<const float32_t *>(W_n2);
  const auto *tw_f = reinterpret_cast<const float32_t *>(tw);

  /*
    Process two howmany transforms at a time. Each iteration first computes the
    length-16 stride-root stage into ZA, stores the twiddled intermediate values
    into scratch, then computes the root-length stride-1 stage into the final
    output.
  */
  for (int r = 0; r < even_howmany; r += 2) {
    const auto *input_row = X_f + r * skip;

    svzero_za();
    calculate_16_stride_root<true>(pg, px, input_row, W_16, skip, stride);
    store_16_stride_root_twiddled<true>(pg, inactive, tw_f, scratch, root,
                                        delta_buffer);

    svzero_za();
    calculate_root_stride_1_pair(pg, px, inactive, W_root, scratch,
                                 Y_f + r * odist * 2, odist, root, stride,
                                 delta_buffer);
  }

  // Tail path for an odd number of howmany transforms.
  if (howmany & 1) {
    const float32_t *input_row = X_f + even_howmany * idist * 2;

    svzero_za();
    calculate_16_stride_root<false>(pg, px, input_row, W_16, skip, stride);
    store_16_stride_root_twiddled<false>(pg, inactive, tw_f, scratch, root,
                                         delta_buffer);

    svzero_za();
    calculate_root_stride_1_single(pg, px, inactive, W_root, scratch,
                                   Y_f + even_howmany * odist * 2, root,
                                   stride);
  }
}

void plfft_16x_cccn_tt_sme2(const std::complex<float> *X,
                            std::complex<float> *Y, int64_t istride,
                            int64_t ostride, const std::complex<float> *W_n1,
                            const std::complex<float> *W_n2,
                            const std::complex<float> *tw, int64_t howmany,
                            int64_t idist, int64_t odist, const int64_t n) {
  // We want to unroll by 2-howmany transform.
  // This is maximum size of buffer that this kernel might require.
  THREAD_LOCAL float32_t scratch[1024];
  plfft_16x_cccn_tt_sme2_impl(X, Y, istride, ostride, W_n1, W_n2, tw, howmany,
                              idist, odist, n, scratch);
}

} // namespace plfft
