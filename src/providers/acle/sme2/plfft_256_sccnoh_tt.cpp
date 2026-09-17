/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft_attrs.hpp"
#include "plfft_complex.hpp"

#include <arm_sme.h>
#include <cassert>
#include <cstdint>

namespace plfft {

static PLFFT_ALWAYS_INLINE void
stage1_along_rows(const float *x, const float *w16, const int64_t skip_input,
                  float *scri0, const int16_t skip_scri,
                  const int16_t offset_scri, const svbool_t ptrue,
                  const svbool_t pfirst) __arm_streaming __arm_inout("za") {
  //------------------------------------------------------------
  // Step 1 - 16 real-to-complex DFTs of length 16. Stride = 16.
  //------------------------------------------------------------
  svzero_za();

  // Stores values as though they are the last vertical slice in the ZA tile
  svfloat32_t last_real0 = svdup_f32(0.0F);
  svfloat32_t last_real1 = svdup_f32(0.0F);
  svfloat32_t last_real2 = svdup_f32(0.0F);
  svfloat32_t last_real3 = svdup_f32(0.0F);

  // Iterate over 16 DFTs of length 16 in this row.
  // Hermitian symmetry, so only need to output the first 9 elements. So only
  // need the first 9 elements from each w16 row.
  // Can handle 4 input rows per iteration because there are 4 ZA tiles when
  // FP32 and SVL=512 and can use 1 tile per row.
  for (int d = 0; d < 16; ++d) {
    // Each row of w16 will be multiplied with a group from each row of the
    // input. 8 complex numbers loaded (16 elements).
    svfloat32_t w16_row = svld1_f32(ptrue, w16);
    // Will load 16 real values each.
    svfloat32_t input_group0 = svld1_f32(ptrue, x);
    svfloat32_t input_group1 = svld1_f32(ptrue, x + skip_input);
    svfloat32_t input_group2 = svld1_f32(ptrue, x + skip_input * 2);
    svfloat32_t input_group3 = svld1_f32(ptrue, x + skip_input * 3);

    // Use outer-product instructions to handle first 8 complex numbers of w16.
    svmopa_za32_f32_m(0, ptrue, ptrue, input_group0, w16_row);
    svmopa_za32_f32_m(1, ptrue, ptrue, input_group1, w16_row);
    svmopa_za32_f32_m(2, ptrue, ptrue, input_group2, w16_row);
    svmopa_za32_f32_m(3, ptrue, ptrue, input_group3, w16_row);

    // Load only real component of 9th complex number in the w16 row.
    svfloat32_t w16_last_real = svdup_n_f32(w16[svcntw()]);

    // Use a vector multiply to handle real and imaginary parts for last
    // complex number used from w16.
    last_real0 = svmla_f32_x(ptrue, last_real0, input_group0, w16_last_real);
    // Repeat for the other rows of input being handled here.
    last_real1 = svmla_f32_x(ptrue, last_real1, input_group1, w16_last_real);
    last_real2 = svmla_f32_x(ptrue, last_real2, input_group2, w16_last_real);
    last_real3 = svmla_f32_x(ptrue, last_real3, input_group3, w16_last_real);

    // Will have accessed 16 elements from x, all real numbers.
    x += svcntw();
    // Will have accessed 18 elements from w16 interleaved real, imag. So skip
    // over the next 7 complex numbers too.
    w16 += 2 * svcntw();
  }

  // Write results to scri.

  float *scri1 = scri0 + skip_scri;
  float *scri2 = scri0 + 2 * skip_scri;
  float *scri3 = scri0 + 3 * skip_scri;
  svbool_t p_elem = pfirst;

  for (int s = 0; s < 16; ++s) {
    svfloat32_t tile0_slice = svread_hor_za32_f32_m(svundef_f32(), ptrue, 0, s);
    svfloat32_t tile1_slice = svread_hor_za32_f32_m(svundef_f32(), ptrue, 1, s);
    svfloat32_t tile2_slice = svread_hor_za32_f32_m(svundef_f32(), ptrue, 2, s);
    svfloat32_t tile3_slice = svread_hor_za32_f32_m(svundef_f32(), ptrue, 3, s);

    // Store 8 complex numbers using the tile slice, then store the ninth from
    // the corresponding last_real accumulator.
    svst1_f32(ptrue, scri0, tile0_slice);
    svst1_f32(ptrue, scri1, tile1_slice);
    svst1_f32(ptrue, scri2, tile2_slice);
    svst1_f32(ptrue, scri3, tile3_slice);

    // Predicate with element s active, rest inactive.
    // Imag part always 0 so don't need to store it.
    svst1_vnum_f32(p_elem, scri0 - s, 1, last_real0);
    svst1_vnum_f32(p_elem, scri1 - s, 1, last_real1);
    svst1_vnum_f32(p_elem, scri2 - s, 1, last_real2);
    svst1_vnum_f32(p_elem, scri3 - s, 1, last_real3);

    p_elem = svpnext_b32(ptrue, p_elem);

    // Increment by 18 elements.
    scri0 += offset_scri;
    scri1 += offset_scri;
    scri2 += offset_scri;
    scri3 += offset_scri;
  }
}

static PLFFT_ALWAYS_INLINE void
stage2_3_along_rows(const float *scri0, const float *scri1,
                    const float *twiddles_pointer, const float *local_w16,
                    const int16_t offset_scri, svfloat32_t *final_element0,
                    svfloat32_t *final_element1, const svbool_t ptrue,
                    const svbool_t pfirst,
                    const svbool_t p2) __arm_streaming __arm_inout("za") {

  svzero_za();

  // Need to reconstruct each row to be able to multiply by the twiddle factors.
  for (int d = 0; d < 16; ++d) {
    // Read 9 complex numbers from scri.
    // Reconstruct to make 16 complex numbers (stored across 2 vectors).
    // Handle 2 rows of the scri at a time.
    // [R0r, R0i, C1r, C1i, ..., C7r, C7i]
    svfloat32_t input0_lo = svld1_f32(ptrue, scri0);
    svfloat32_t input1_lo = svld1_f32(ptrue, scri1);
    // [R8r, 0, 0, ...]
    svfloat32_t input0_last = svld1_vnum_f32(pfirst, scri0, 1);
    svfloat32_t input1_last = svld1_vnum_f32(pfirst, scri1, 1);

    // Form [C1, ..., C8], then reverse the 64-bit complex lanes to reconstruct
    // the half of the Hermitian sequence that was not written.
    svfloat32_t input0_hi = svreinterpret_f32_u64(
        svrev_u64(svreinterpret_u64_f32(svext_f32(input0_lo, input0_last, 2))));
    svfloat32_t input1_hi = svreinterpret_f32_u64(
        svrev_u64(svreinterpret_u64_f32(svext_f32(input1_lo, input1_last, 2))));
    // The reconstructed high halves are conjugated as part of the complex
    // multiplication below by using rotations 0 and 270.

    //--------------------------------------------------
    // Step 2 - Read from scri and apply twiddle factors - just for 2 rows of
    // the SCRI out of 4.
    //--------------------------------------------------

    // 32 twiddle factors fit into two vectors
    svfloat32_t tw_lo = svld1_vnum_f32(ptrue, twiddles_pointer, 0);
    svfloat32_t tw_hi = svld1_vnum_f32(ptrue, twiddles_pointer, 1);

    svfloat32_t tw_in0_lo = svdup_f32(0.0F);
    svfloat32_t tw_in0_hi = svdup_f32(0.0F);
    svfloat32_t tw_in1_lo = svdup_f32(0.0F);
    svfloat32_t tw_in1_hi = svdup_f32(0.0F);

    // Multiply lower and upper halves by the lower and upper halves of the
    // twiddle row vector.
    tw_in0_lo = svcmla_f32_x(ptrue, tw_in0_lo, input0_lo, tw_lo, 0);
    tw_in0_lo = svcmla_f32_x(ptrue, tw_in0_lo, input0_lo, tw_lo, 90);
    tw_in0_hi = svcmla_f32_x(ptrue, tw_in0_hi, input0_hi, tw_hi, 0);
    tw_in0_hi = svcmla_f32_x(ptrue, tw_in0_hi, input0_hi, tw_hi, 270);

    tw_in1_lo = svcmla_f32_x(ptrue, tw_in1_lo, input1_lo, tw_lo, 0);
    tw_in1_lo = svcmla_f32_x(ptrue, tw_in1_lo, input1_lo, tw_lo, 90);
    tw_in1_hi = svcmla_f32_x(ptrue, tw_in1_hi, input1_hi, tw_hi, 0);
    tw_in1_hi = svcmla_f32_x(ptrue, tw_in1_hi, input1_hi, tw_hi, 270);

    //------------------------------------------
    // Step 3 - 8 DFTs of length 16. Stride = 1.
    //------------------------------------------
    // Only need to compute the results for 8 out of the 16 rows (as well as the
    // final element), as the rest of the row output is implied by Hermitian
    // symmetry.

    // First 16 elements of w16 row used in outer product loop.
    svfloat32_t w16_row = svld1_f32(ptrue, local_w16);

    // Swap order of arguments to simulate a matrix transpose.
    svmopa_za32_f32_m(0, ptrue, ptrue, w16_row, tw_in0_lo);
    svmopa_za32_f32_m(1, ptrue, ptrue, w16_row, tw_in0_hi);
    svmopa_za32_f32_m(2, ptrue, ptrue, w16_row, tw_in1_lo);
    svmopa_za32_f32_m(3, ptrue, ptrue, w16_row, tw_in1_hi);

    // Process the multiplications for the last element. Should be w16[8]
    // multiplied by what occupies the first element in the input each
    // iteration. This element is always real.
    svfloat32_t final_w16 = svld1_vnum_f32(p2, local_w16, 1);
    *final_element0 = svmla_f32_x(p2, *final_element0, final_w16, tw_in0_lo);
    *final_element1 = svmla_f32_x(p2, *final_element1, final_w16, tw_in1_lo);

    // Point to next row of twiddle factors and in w16 matrix. There are 32
    // elements per row.
    twiddles_pointer += 2 * svcntw();
    local_w16 += 2 * svcntw();
    // Point to 18 elements from each of the first two rows in the SCRI.
    scri0 += offset_scri;
    scri1 += offset_scri;
  }
}

static PLFFT_ALWAYS_INLINE void
stage3_write(float *out0, float *out1, svfloat32_t final_element0,
             svfloat32_t final_element1, const svbool_t p2,
             const svbool_t ptrue) __arm_streaming __arm_inout("za") {
  // Store the output for 2 rows at a time.
  for (int s = 0; s < 16; s += 2) {
    // tile0_s0 will have [W0rIn0r, W0rIn0i, ...].
    // tile0_s1 will have [W0iIn0r, W0iIn0i, ...].
    // Rotate slice1 by 90 degrees and add the two vectors.
    // Result: [W0rIn0r - W0iIn0i, W0rIn0i + W0iIn0r, ...].
    // So will be stored interleaved real, imag.

    // tile0 and tile1 contain the output for the first row of w16 out of the
    // two handled by this function.
    svfloat32x2_t tile0_s0_s1 = svread_hor_za32_f32_vg2(0, s);
    svfloat32x2_t tile1_s0_s1 = svread_hor_za32_f32_vg2(1, s);
    // tile2 and tile3 contain the output for the second row of w16 out of the
    // two handled by this function.
    svfloat32x2_t tile2_s0_s1 = svread_hor_za32_f32_vg2(2, s);
    svfloat32x2_t tile3_s0_s1 = svread_hor_za32_f32_vg2(3, s);

    // Add slice0 to slice1 rotated by 90 degrees.
    svfloat32_t result0 = svcadd_f32_x(ptrue, svget2_f32(tile0_s0_s1, 0),
                                       svget2_f32(tile0_s0_s1, 1), 90);
    svfloat32_t result1 = svcadd_f32_x(ptrue, svget2_f32(tile1_s0_s1, 0),
                                       svget2_f32(tile1_s0_s1, 1), 90);
    svfloat32_t result2 = svcadd_f32_x(ptrue, svget2_f32(tile2_s0_s1, 0),
                                       svget2_f32(tile2_s0_s1, 1), 90);
    svfloat32_t result3 = svcadd_f32_x(ptrue, svget2_f32(tile3_s0_s1, 0),
                                       svget2_f32(tile3_s0_s1, 1), 90);

    // These results are for two rows of the output. One tile slice will have
    // half of a group of 16 complex numbers (interleaved).
    svst1_vnum_f32(ptrue, out0, 0, result0);
    svst1_vnum_f32(ptrue, out0, 1, result1);
    // Repeat for next row.
    svst1_vnum_f32(ptrue, out1, 0, result2);
    svst1_vnum_f32(ptrue, out1, 1, result3);

    if (s == 14) {
      // Write the final element for each of the two rows.
      svst1_vnum_f32(p2, out0, 2, final_element0);
      svst1_vnum_f32(p2, out1, 2, final_element1);
    }

    // Increment by 2 VL as we store 2 interleaved vectors per row of output
    // here.
    out0 += 2 * svcntw();
    out1 += 2 * svcntw();
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA static void plfft_256_sccnoh_tt_sme2_impl(
    const float *input, complex_float *output, [[maybe_unused]] int64_t istride,
    [[maybe_unused]] int64_t ostride, const complex_float *W_n1_cmplx,
    [[maybe_unused]] const complex_float *W_n2_cmplx,
    const complex_float *tw_cmplx, int64_t howmany, int64_t idist,
    int64_t odist, [[maybe_unused]] const int64_t n, float *scri) {
  assert(svcntw() == 16);

  const auto *W = reinterpret_cast<const float *>(W_n1_cmplx);
  const auto *tw = reinterpret_cast<const float *>(tw_cmplx);

  // Will be 9 complex numbers per DFT output stored, the other 7 are implied by
  // Hermitian symmetry.
  const int16_t offset_scri = 18;
  // Each row of input is 256 real numbers.
  const int64_t skip_input = idist;
  // Add one row of elements when writing to scri (real, imag so multiply by 2).
  // One row in scri is 16 * 9 = 144 complex numbers.
  const int16_t skip_scri = 288;
  // Output will be 129 complex numbers = 258 elements.
  const int64_t skip_output = 2 * odist;

  const svbool_t ptrue = svptrue_b32();
  const svbool_t p2 = svptrue_pat_b32(SV_VL2);
  const svbool_t pfirst = svptrue_pat_b32(SV_VL1);
  for (int64_t r = 0; r < howmany; r += 4) {
    const float *x = &input[r * skip_input];

    // This function call also writes to the scri.
    stage1_along_rows(x, W, skip_input, scri, skip_scri, offset_scri, ptrue,
                      pfirst);

    const auto *scri0 = reinterpret_cast<const float *>(scri);
    const auto *scri1 = scri0 + skip_scri;
    const auto *scri2 = scri0 + skip_scri * 2;
    const auto *scri3 = scri0 + skip_scri * 3;

    auto *out0 = reinterpret_cast<float *>(&output[r * odist]);
    auto *out1 = out0 + skip_output;
    auto *out2 = out0 + 2 * skip_output;
    auto *out3 = out0 + 3 * skip_output;
    svfloat32_t final_element0 = svdup_f32(0.0F);
    svfloat32_t final_element1 = svdup_f32(0.0F);

    // Two of each function call as each pair will handle 2 rows from the SCRI.
    // There are 4 rows in the SCRI to handle.
    // ZA tiles should be preserved between these two functions calls.
    stage2_3_along_rows(scri0, scri1, tw, W, offset_scri, &final_element0,
                        &final_element1, ptrue, pfirst, p2);
    stage3_write(out0, out1, final_element0, final_element1, p2, ptrue);

    svfloat32_t final_element2 = svdup_f32(0.0F);
    svfloat32_t final_element3 = svdup_f32(0.0F);

    // ZA tiles should be preserved between these two function calls. Reset to
    // zero at the start of this function call.
    stage2_3_along_rows(scri2, scri3, tw, W, offset_scri, &final_element2,
                        &final_element3, ptrue, pfirst, p2);
    stage3_write(out2, out3, final_element2, final_element3, p2, ptrue);
  }
}

void plfft_256_sccnoh_tt_sme2(const float *input, complex_float *output,
                              int64_t istride, int64_t ostride,
                              const complex_float *W_n1_cmplx,
                              const complex_float *W_n2_cmplx,
                              const complex_float *tw_cmplx, int64_t howmany,
                              int64_t idist, int64_t odist, const int64_t n) {
  // scri will store 4 rows at a time. Will have 144 complex numbers per row.
  THREAD_LOCAL static float scri[1152];
  plfft_256_sccnoh_tt_sme2_impl(input, output, istride, ostride, W_n1_cmplx,
                                W_n2_cmplx, tw_cmplx, howmany, idist, odist, n,
                                scri);
}

} // namespace plfft
