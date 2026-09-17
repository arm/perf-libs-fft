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

template<int RealLane, int ImagLane>
PLFFT_ALWAYS_INLINE void store_twiddled_interleaved(
    const svbool_t pg, const svfloat32_t value_re, const svfloat32_t value_im,
    const svfloat32_t twiddle_lanes, float32_t *out) __arm_streaming {
  svfloat32_t out_re = svmul_lane_f32(value_re, twiddle_lanes, RealLane);
  svfloat32_t out_im = svmul_lane_f32(value_im, twiddle_lanes, RealLane);
  out_re = svmls_lane_f32(out_re, value_im, twiddle_lanes, ImagLane);
  out_im = svmla_lane_f32(out_im, value_re, twiddle_lanes, ImagLane);
  svst2_f32(pg, out, svcreate2(out_re, out_im));
}

/* Given a batch of 16, this function:
   - calculates the 16-long FFTs of stride = root. It can calculate either 1 or
   2 batches of 16 FFTs at the same time, depending on the template parameter.
   Tiles za0, za1 are used for the first batch (starting at the starting point
   passed in) while tiles za2, za3 are used for the second batch (starting at
   row_tiles_01_in + skip_in).
   - extracts the calculated FFTs from the tiles and multiplies them by the
   twiddle factors.
   - writes the results into the temporary buffer, starting at row_tiles_01_out.
   It stores the results with an ostride = root.

   On the twiddle factors.
   To explain the layout of the matrix, let's start from the normal layout:
       TW = [ tw(0,0)    tw(0,1)    ... tw(0,15)
              tw(1,0)    tw(1,1)    ... tw(1,15)
              ...
              tw(root - 1,0) tw(root - 1,1) ... tw(root - 1,15)]

   where the matrix is complex and has dimensions (root, 16). For this kernel,
   the layout is the following:

       TW = [ tw(0,0)        tw(1,0)    tw(0,1)  tw(1,1)  ... tw(0,7) tw(1,7)
              tw(0,8)        tw(1,8)    tw(0,9)  tw(1,9)  ... tw(0,15) tw(1,15)
              ...
              tw(root - 2,0) tw(root - 1,0) ... tw(root - 2,7)  tw(root - 1,7)
              tw(root - 2,8) tw(root - 1,8) ... tw(root - 2,15) tw(root - 1,15)]
   When the root is even, the matrix looks like the one above;
   when the root is odd, the code interleaves the row with (0,0) values. Those
   rows would multiply the za2,za3 tiles which would not be used (because
   UnrollTwo = false) and therefore, they don't affect the final result.
*/
template<bool UnrollTwo>
PLFFT_ALWAYS_INLINE void calculate_16_stride_root_and_twiddle(
    svbool_t p_data, const float32_t *row_tiles_01_in,
    float32_t *row_tiles_01_out, const float32_t *w_ptr,
    const float32_t *tw_ri_pts, const int64_t root, const int64_t skip_in,
    const int64_t skip_out, const int64_t delta_batch_in,
    const int64_t delta_batch_out) __arm_streaming __arm_inout("za") {

  svbool_t pg = svptrue_b32();

  const float32_t *row_tiles_23_in = nullptr;
  float32_t *row_tiles_23_out = nullptr;
  if constexpr (UnrollTwo) {
    row_tiles_23_in = row_tiles_01_in + skip_in;
    row_tiles_23_out = row_tiles_01_out + skip_out;
  }

  /* Calculates a batch of 16 FFTs of length 16 and stride = root. If the
     function is instantiated with UnrollTwo = true, then the function
     calculates 2 batches. The first batch ends up in za0,za1 and the second
     batch in za2, za3. The first batch starts at row_tiles_01_in and the second
     batch starts at row_tiles_01_in + skip_in.

     As first stage, for a 16x-long FFT, we have x 16-points FFTs. Therefore the
     function is instantiated with UnrollTwo = true when we can use the four
     tiles to calculate two transforms at the same time and with UnrollTwo =
     false when there is only one 16-long FFT and we only need 2 tiles.

     Below:
      - dft_re_im stores in 1 register the real part of the DFT coefficients and
        the imaginary part in second vector.
      - data_tr1 contains the input data for the first batch
      - data_tr2 contains the input data for the second batch when UnrollTwo =
     true

     data_tr1 and data_tr2 represent the same index points, just of two
     different batches and therefore, they multiply the same DFT coefficients.
     Therefore, no need to have separate DFT coefficient registers.

     After each outer product, I advance the data pointers by the right
     quantities to point to the next point of the same FFT in the same batch,
     and advance w_ptr to the next 16 complex DFT coefficients.
  */

  /* First iteration - only real part of dft matrix (w[0] = 1+0j)*/
  svfloat32x2_t dft_re_im = svld2_f32(pg, w_ptr);
  svfloat32x2_t data_tr1 = svld2_f32(p_data, row_tiles_01_in);
  svfloat32x2_t data_tr2;

  svmopa_za32_f32_m(0, pg, p_data, svget2(dft_re_im, 0),
                    svget2(data_tr1, 0)); // za0: real * real
  svmopa_za32_f32_m(1, pg, p_data, svget2(dft_re_im, 0),
                    svget2(data_tr1, 1)); // za1: imag * real
  row_tiles_01_in += delta_batch_in;

  if constexpr (UnrollTwo) {
    data_tr2 = svld2_f32(p_data, row_tiles_23_in);
    svmopa_za32_f32_m(2, pg, p_data, svget2(dft_re_im, 0),
                      svget2(data_tr2, 0)); // za2: real * real
    svmopa_za32_f32_m(3, pg, p_data, svget2(dft_re_im, 0),
                      svget2(data_tr2, 1)); // za3: imag * real
    row_tiles_23_in += delta_batch_in;
  }
  w_ptr += 32;

  // Remaining points to process
  for (int pt = 1; pt < 16; pt++) {
    dft_re_im = svld2_f32(pg, w_ptr);
    data_tr1 = svld2_f32(p_data, row_tiles_01_in);
    accumulate_complex_outer_to_za<0, 1>(pg, p_data, dft_re_im, data_tr1);
    row_tiles_01_in += delta_batch_in;

    if constexpr (UnrollTwo) {
      data_tr2 = svld2_f32(p_data, row_tiles_23_in);
      accumulate_complex_outer_to_za<2, 3>(pg, p_data, dft_re_im, data_tr2);
      row_tiles_23_in += delta_batch_in;
    }

    w_ptr += 32;
  }

  // Extract the results from the tiles and multiply by the twiddle
  // factors. We can use one loop because the transforms are of length 16  so we
  // know all the tiles are full of values to read.
  for (int slice = 0; slice < 16; slice += 4) {
    svfloat32x4_t re_batch1 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t im_batch1 = svread_hor_za32_f32_vg4(1, slice);
    svfloat32x4_t re_batch2;
    svfloat32x4_t im_batch2;

    if constexpr (UnrollTwo) {
      re_batch2 = svread_hor_za32_f32_vg4(2, slice);
      im_batch2 = svread_hor_za32_f32_vg4(3, slice);
    }

    /*
      Allow for some abuse of notation and implicit mixing of real
      and complex multiplications here below.

      Based on the outer-product formula, the row index of the tile
      corresponds to the point index (from 0 to 15) of one of the
      16 transforms in the batch. The columns index corresponds to index of
      the transform in the batch.

      Therefore, in a usual layout of the twiddle factors, the first element
      of the first row needs to multiply the first row (across all columns)
      of the za0 and za1 tiles, the second element of the first row will
      multiply the second row of the tiles and so on.

      Because za0,za1 and za2,za3 are indexed the same, given the interleaved
      layout of the twiddle factors, the first 2 elements of the first row
      will multiply the first row of za0, za1 (for tw(0,0)) and za2, za3 (for
      tw(1,0)).

      When reading the 4 elements and duplicating them across the vector with
      the intrinsic below we get a vector of this form: v = [ Re(tw(0,0)) ...
      Re(tw(0,0)) Im(tw(0,0)) ... Im(tw(0,0)) Re(tw(1,0)) ... Re(tw(1,0))
      Im(tw(1,0)) ... Im(tw(1,0))] where each group is 16 long. So every time we
      process a new row of the tile, we have to advance the tw_ri_pts pointer
      because we read 4 floats every time.
    */

    svfloat32_t re01_im01_re23_im23 = svld1rq_f32(pg, tw_ri_pts);
    store_twiddled_interleaved<0, 1>(p_data, svget4(re_batch1, 0),
                                     svget4(im_batch1, 0), re01_im01_re23_im23,
                                     row_tiles_01_out);
    row_tiles_01_out += delta_batch_out;

    if constexpr (UnrollTwo) {
      store_twiddled_interleaved<2, 3>(p_data, svget4(re_batch2, 0),
                                       svget4(im_batch2, 0),
                                       re01_im01_re23_im23, row_tiles_23_out);
      row_tiles_23_out += delta_batch_out;
    }

    // This will multiply the svget4(,1)
    re01_im01_re23_im23 = svld1rq_f32(pg, tw_ri_pts + 4);
    store_twiddled_interleaved<0, 1>(p_data, svget4(re_batch1, 1),
                                     svget4(im_batch1, 1), re01_im01_re23_im23,
                                     row_tiles_01_out);
    row_tiles_01_out += delta_batch_out;

    if constexpr (UnrollTwo) {
      store_twiddled_interleaved<2, 3>(p_data, svget4(re_batch2, 1),
                                       svget4(im_batch2, 1),
                                       re01_im01_re23_im23, row_tiles_23_out);
      row_tiles_23_out += delta_batch_out;
    }

    // This will multiply the svget4(,2)
    re01_im01_re23_im23 = svld1rq_f32(pg, tw_ri_pts + 8);
    store_twiddled_interleaved<0, 1>(p_data, svget4(re_batch1, 2),
                                     svget4(im_batch1, 2), re01_im01_re23_im23,
                                     row_tiles_01_out);
    row_tiles_01_out += delta_batch_out;

    if constexpr (UnrollTwo) {
      store_twiddled_interleaved<2, 3>(p_data, svget4(re_batch2, 2),
                                       svget4(im_batch2, 2),
                                       re01_im01_re23_im23, row_tiles_23_out);
      row_tiles_23_out += delta_batch_out;
    }

    // This will multiply the svget4(,3)
    re01_im01_re23_im23 = svld1rq_f32(pg, tw_ri_pts + 12);
    store_twiddled_interleaved<0, 1>(p_data, svget4(re_batch1, 3),
                                     svget4(im_batch1, 3), re01_im01_re23_im23,
                                     row_tiles_01_out);
    row_tiles_01_out += delta_batch_out;

    if constexpr (UnrollTwo) {
      store_twiddled_interleaved<2, 3>(p_data, svget4(re_batch2, 3),
                                       svget4(im_batch2, 3),
                                       re01_im01_re23_im23, row_tiles_23_out);
      row_tiles_23_out += delta_batch_out;
    }
    tw_ri_pts += 16;
  }
}

PLFFT_ALWAYS_INLINE void save_transform(const svbool_t pg, const int64_t offset,
                                        svfloat32_t real, svfloat32_t imag,
                                        float32_t *out) __arm_streaming {
  svst2_f32(pg, out + offset, svcreate2(real, imag));
}

/*
  This function calculates a batch of 16 FFTs, all root-long with stride 1. In
  each batch of 16, there are 16 transforms to calculate, because each
  individual FFT is long 16*root. For this reason, we know we can always unroll
  by two, therefore using za0,za1 and za2,za3 to calculate two batches of 16 of
  root-long FFTs.
*/

PLFFT_ALWAYS_INLINE void calculate_root_stride_1(
    svbool_t p_data, const float32_t *row_tiles_01_in, float32_t *row_01_out,
    const float32_t *w_ptr, const int64_t root, const int64_t group,
    const int64_t skip_in, const int64_t delta_batch_in, const int64_t skip_out,
    const int64_t delta_batch_out) __arm_streaming __arm_inout("za") {

  svbool_t pg = svptrue_b32();
  svbool_t px = svwhilelt_b32_s64(0, root);
  svfloat32_t inactive = svdup_f32(0.0f);

  /*
    Below the suffix _01 indicates the data that will end up in za0,za1 and
    the suffix _23 indicates the data that will end up in za2,za3.
  */
  const float32_t *row_tiles_23_in = row_tiles_01_in + delta_batch_in;

  svfloat32x2_t coeff = svld2_f32(px, w_ptr);
  svfloat32x2_t data_tr1 = svld2_f32(p_data, row_tiles_01_in);
  svfloat32x2_t data_tr2 = svld2_f32(p_data, row_tiles_23_in);

  svmopa_za32_f32_m(0, px, p_data, svget2(coeff, 0), svget2(data_tr1, 0));
  svmopa_za32_f32_m(1, px, p_data, svget2(coeff, 0), svget2(data_tr1, 1));
  svmopa_za32_f32_m(2, px, p_data, svget2(coeff, 0), svget2(data_tr2, 0));
  svmopa_za32_f32_m(3, px, p_data, svget2(coeff, 0), svget2(data_tr2, 1));

  row_tiles_01_in += skip_in;
  row_tiles_23_in += skip_in;
  w_ptr += root * 2;

  for (int pt = 1; pt < root; ++pt) {
    coeff = svld2_f32(px, w_ptr);
    data_tr1 = svld2_f32(p_data, row_tiles_01_in);
    data_tr2 = svld2_f32(p_data, row_tiles_23_in);

    accumulate_complex_outer_to_za<0, 1>(px, p_data, coeff, data_tr1);
    accumulate_complex_outer_to_za<2, 3>(px, p_data, coeff, data_tr2);

    row_tiles_01_in += skip_in;
    row_tiles_23_in += skip_in;
    w_ptr += root * 2;
  }

  /*
    We only have root rows to extract and to write in the output in a transposed
    way. If we consider tiles za0 and za1, they contain on the row index, the
    point index of the FFT and on the column index the index of the transform in
    the batch of 16. Therefore, we can read each row from both, interleave them
    and then write them to the correct place in the output buffer, once we have
    moved to the right place to indicate transposition.

    We try to minimize the number of times we read from the tiles by unroll by
    4, 2, and then use an individual read for the last row if row is odd.
    Because the batches are of 16 transforms we can safely use a ptrue() to
    write to the output buffer. The data in za0, za1  correspond to index group,
    za2 and za3 correspond to index group + 1.
  */
  int32_t slice = 0;
  const int32_t unroll_4 = root / 4;
  for (int32_t ur = 0; ur < unroll_4; ur++) {
    svfloat32x4_t res0 = svread_hor_za32_f32_vg4(0, slice);
    svfloat32x4_t res1 = svread_hor_za32_f32_vg4(1, slice);
    svfloat32x4_t res2 = svread_hor_za32_f32_vg4(2, slice);
    svfloat32x4_t res3 = svread_hor_za32_f32_vg4(3, slice);

    save_transform(p_data, (16 * slice + group) * skip_out, svget4(res0, 0),
                   svget4(res1, 0), row_01_out);
    save_transform(p_data, (16 * (slice + 1) + group) * skip_out,
                   svget4(res0, 1), svget4(res1, 1), row_01_out);
    save_transform(p_data, (16 * (slice + 2) + group) * skip_out,
                   svget4(res0, 2), svget4(res1, 2), row_01_out);
    save_transform(p_data, (16 * (slice + 3) + group) * skip_out,
                   svget4(res0, 3), svget4(res1, 3), row_01_out);

    save_transform(p_data, (16 * slice + (group + 1)) * skip_out,
                   svget4(res2, 0), svget4(res3, 0), row_01_out);
    save_transform(p_data, (16 * (slice + 1) + (group + 1)) * skip_out,
                   svget4(res2, 1), svget4(res3, 1), row_01_out);
    save_transform(p_data, (16 * (slice + 2) + (group + 1)) * skip_out,
                   svget4(res2, 2), svget4(res3, 2), row_01_out);
    save_transform(p_data, (16 * (slice + 3) + (group + 1)) * skip_out,
                   svget4(res2, 3), svget4(res3, 3), row_01_out);

    slice += 4;
  }

  const int32_t unroll_2 = (root % 4) / 2;
  if (unroll_2) {
    for (int32_t ur = 0; ur < unroll_2; ur++) {
      svfloat32x2_t res0 = svread_hor_za32_f32_vg2(0, slice);
      svfloat32x2_t res1 = svread_hor_za32_f32_vg2(1, slice);
      svfloat32x2_t res2 = svread_hor_za32_f32_vg2(2, slice);
      svfloat32x2_t res3 = svread_hor_za32_f32_vg2(3, slice);

      save_transform(p_data, (16 * slice + group) * skip_out, svget2(res0, 0),
                     svget2(res1, 0), row_01_out);
      save_transform(p_data, (16 * (slice + 1) + group) * skip_out,
                     svget2(res0, 1), svget2(res1, 1), row_01_out);
      save_transform(p_data, (16 * slice + (group + 1)) * skip_out,
                     svget2(res2, 0), svget2(res3, 0), row_01_out);
      save_transform(p_data, (16 * (slice + 1) + (group + 1)) * skip_out,
                     svget2(res2, 1), svget2(res3, 1), row_01_out);

      slice += 2;
    }
  }

  const int32_t last_row = root & 1;
  if (last_row) {
    svfloat32_t res0 = svread_hor_za32_f32_m(inactive, pg, 0, slice);
    svfloat32_t res1 = svread_hor_za32_f32_m(inactive, pg, 1, slice);
    svfloat32_t res2 = svread_hor_za32_f32_m(inactive, pg, 2, slice);
    svfloat32_t res3 = svread_hor_za32_f32_m(inactive, pg, 3, slice);

    save_transform(p_data, (16 * slice + group) * skip_out, res0, res1,
                   row_01_out);
    save_transform(p_data, (16 * slice + (group + 1)) * skip_out, res2, res3,
                   row_01_out);
  }
}

PLFFT_ALWAYS_INLINE void
calculate_group(const int n, const float32_t *X, float32_t *Y, int64_t istride,
                int64_t ostride, const float32_t *W_16, const float32_t *W_root,
                const float32_t *tw, const svbool_t p_howmany,
                float32_t *buffer) __arm_streaming __arm_inout("za") {
  constexpr int32_t radix = 16;
  /*This kernel only computes FFTs of length 16x, where x, the multiplicative
    factor, is indicated by the variable `root`*/
  const int64_t root = n / radix;
  /*Let's split root into an even part (unrolled by two) and the remainder */
  const int64_t even_root = root & ~1;
  const int64_t odd_root = root & 1;
  /*Skip quantities to find the next point in the same transform in
    the input buffer and output buffer. This just transforms a stride on complex
    values into a stride on real values.*/
  const int64_t skip_in = istride * 2;
  const int64_t skip_out = ostride * 2;
  /*Transformation of the input/output strides when considering a transform
    with stride = root.*/
  const int64_t delta_batch_in = skip_in * root;
  const int64_t delta_batch_out = skip_out * root;

  /* This can be hardcoded because it is a measure of skip
     on the internal buffer so we must skip of the number of
     columns in the buffer. This are the strides on the internal
     buffer.*/
  const int64_t skip_out_buffer = 16 * 2;
  const int64_t delta_batch_out_buffer = skip_out_buffer * root;

  const float32_t *input_point = nullptr;
  float32_t *output_point = nullptr;
  /*Twiddles interleaved points = tw_ri_pts*/
  const float32_t *tw_ri_pts = nullptr;

  /* FFTs of length 16 and stride = root & multiply by the twiddle.
     Outputting into the buffer.
  */
  for (int row = 0; row < even_root; row += 2) {
    svzero_za();
    tw_ri_pts = tw + row * 32;
    input_point = X + row * skip_in;
    output_point = buffer + row * skip_out_buffer;
    calculate_16_stride_root_and_twiddle<true>(
        p_howmany, input_point, output_point, W_16, tw_ri_pts, root, skip_in,
        skip_out_buffer, delta_batch_in, delta_batch_out_buffer);
  }
  if (odd_root) {
    svzero_za();
    input_point = X + even_root * skip_in;
    output_point = buffer + even_root * skip_out_buffer;
    tw_ri_pts = tw + even_root * 32;
    calculate_16_stride_root_and_twiddle<false>(
        p_howmany, input_point, output_point, W_16, tw_ri_pts, root, skip_in,
        skip_out_buffer, delta_batch_in, delta_batch_out_buffer);
  }

  // FFTs of length root and stride = 1 & transposing into result array
  for (int group = 0; group < 16; group += 2) {
    input_point = buffer + group * delta_batch_out_buffer;
    output_point = Y;

    // Zero the ZA array
    svzero_za();
    calculate_root_stride_1(p_howmany, input_point, output_point, W_root, root,
                            group, skip_out_buffer, delta_batch_out_buffer,
                            skip_out, delta_batch_out);
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA static void plfft_16x_cccn_uu_sme2_impl(
    const std::complex<float> *X, std::complex<float> *Y, int64_t istride,
    int64_t ostride, const std::complex<float> *W_n1,
    const std::complex<float> *W_n2, const std::complex<float> *tw,
    int64_t howmany, int64_t /*idist*/, int64_t /*odist*/, const int64_t n,
    float32_t *buffer) {
  // This kernel is currently specialized for a 512 bits Scalar Vector Length
  // which corresponds to 16 float32 elements per vector.
  assert(svcntw() == 16);

  /* The for loop below splits the `howmany` batch into batches of
     up to 16 FFTs each. Within each batch, the FFTs are of length 16x.
     */

  const auto *W_16 = reinterpret_cast<const float32_t *>(W_n1);
  const auto *W_root = reinterpret_cast<const float32_t *>(W_n2);

  for (int col_group = 0; col_group < howmany; col_group += 16) {
    svbool_t p_data = svwhilelt_b32_s64(col_group, howmany);
    const auto *tw_ri_f = reinterpret_cast<const float32_t *>(tw);
    const auto *x_f_in = reinterpret_cast<const float32_t *>(&X[col_group]);
    auto *y_f_out = reinterpret_cast<float32_t *>(&Y[col_group]);

    calculate_group(n, x_f_in, y_f_out, istride, ostride, W_16, W_root, tw_ri_f,
                    p_data, buffer);
  }
}

void plfft_16x_cccn_uu_sme2(const std::complex<float> *X,
                            std::complex<float> *Y, int64_t istride,
                            int64_t ostride, const std::complex<float> *W_n1,
                            const std::complex<float> *W_n2,
                            const std::complex<float> *tw, int64_t howmany,
                            int64_t idist, int64_t odist, const int64_t n) {
  /* This is the largest buffer we may require:
   256 (rows - max FFT length)* 16 (cols - batch size) * 2 (real and imag);
   The point of the buffer is to hold the intermediate results of the 16-long
   FFT with stride=root and multiplied by the twiddle. In this way, when we
   write to output location we can also safely transpose without worrying */
  THREAD_LOCAL float32_t buffer[8192];
  plfft_16x_cccn_uu_sme2_impl(X, Y, istride, ostride, W_n1, W_n2, tw, howmany,
                              idist, odist, n, buffer);
}

} // namespace plfft
