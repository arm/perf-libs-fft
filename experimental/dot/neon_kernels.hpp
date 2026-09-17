/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/*
 * X = Fx for N = 8, vectorized dot product kernels
 *
 * Q0.7 inputs -> Q0.14 intermediate products -> Q0.7 outputs
 */
#pragma once

#include <arm_neon.h>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string.h>

using cmplx_int8_t = std::complex<int8_t>;

namespace dft1d_neon {

constexpr int8_t a = 127;
constexpr int8_t b = -128;
constexpr int8_t c = 91;  // Q0.7 rep of +1/sqrt(2)
constexpr int8_t d = -91; // Q0.7 rep of -1/sqrt(2)

/* Real and imaginary parts of the 8x8 DFT matrix */
// clang-format off
alignas(16) constexpr int8_t f1_n8[128] = {
    a, a, a, a, a, a, a, a, 0, 0, 0, 0, 0, 0, 0, 0,
    a, c, 0, d, b, d, 0, c, 0, c, a, c, 0, d, b, d,
    a, 0, b, 0, a, 0, b, 0, 0, a, 0, b, 0, a, 0, b,
    a, d, 0, c, b, c, 0, d, 0, c, b, c, 0, d, a, d,
    a, b, a, b, a, b, a, b, 0, 0, 0, 0, 0, 0, 0, 0,
    a, d, 0, c, b, c, 0, d, 0, d, a, d, 0, c, b, c,
    a, 0, b, 0, a, 0, b, 0, 0, b, 0, a, 0, b, 0, a,
    a, c, 0, d, b, d, 0, c, 0, d, b, d, 0, c, a, c,
};

alignas(16) constexpr int8_t f2_n8[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, a, a, a, a, a, a, a, a,
    0, d, b, d, 0, c, a, c, a, c, 0, d, b, d, 0, c,
    0, b, 0, a, 0, b, 0, a, a, 0, b, 0, a, 0, b, 0,
    0, d, a, d, 0, c, b, c, a, d, 0, c, b, c, 0, d,
    0, 0, 0, 0, 0, 0, 0, 0, a, b, a, b, a, b, a, b,
    0, c, b, c, 0, d, a, d, a, d, 0, c, b, c, 0, d,
    0, a, 0, b, 0, a, 0, b, a, 0, b, 0, a, 0, b, 0,
    0, c, a, c, 0, d, b, d, a, c, 0, d, b, d, 0, c,
};

/**
 * Packed constants for the sym2 kernel. The input permutation
 * is [r_even, i_even, r_odd, i_odd]. f2 is derived from
 * f1 by swapping adjacent 32-bit lanes, so it is not stored
 */
alignas(64) constexpr int8_t n8_sym2_tables[48] = {
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15,
    a, 0, b, 0, 0, b, 0, a, c, d, d, c, d, d, c, c,
    a, b, a, b, a, b, a, b, b, a, b, a, a, b, a, b,
};

alignas(16) constexpr uint8_t cmplx_pair_reverse_indices[16] = {
    12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
};

constexpr int32_t cmplx_pair_signs[4] = { 1, -1, 1, -1 };

constexpr int32_t n8_sym_signs4[4] = { 1, -1, -1, 1 };
// clang-format on

struct n8_dft_vectors {
  int8x16_t f1[8];
  int8x16_t f2[8];
};

struct n8_sym_vectors {
  int8x16_t f1[3];
  int8x16_t f2[3];
  int32x4_t signs4;
  uint8x16_t indices;
  int32x4_t cmplx_signs;
};

struct n8_sym2_vectors {
  uint8x16_t indices;
  int8x16_t f1;
  int8x16_t f2;
  int8x16_t f3;
};

struct hadamard32x2x2 {
  int32x2_t h02; // h0, h2
  int32x2_t h13; // h1, h3
};

struct hadamard32x4x2 {
  int32x4_t h01; // {x_h0, y_h0, x_h1, y_h1}
  int32x4_t h23; // {x_h2, y_h2, x_h3, y_h3}
};

__attribute__((always_inline)) static inline n8_dft_vectors
load_n8_dft_vectors() {
  return {
      {
          vld1q_s8(f1_n8),
          vld1q_s8(f1_n8 + 16),
          vld1q_s8(f1_n8 + 32),
          vld1q_s8(f1_n8 + 48),
          vld1q_s8(f1_n8 + 64),
          vld1q_s8(f1_n8 + 80),
          vld1q_s8(f1_n8 + 96),
          vld1q_s8(f1_n8 + 112),
      },
      {
          vld1q_s8(f2_n8),
          vld1q_s8(f2_n8 + 16),
          vld1q_s8(f2_n8 + 32),
          vld1q_s8(f2_n8 + 48),
          vld1q_s8(f2_n8 + 64),
          vld1q_s8(f2_n8 + 80),
          vld1q_s8(f2_n8 + 96),
          vld1q_s8(f2_n8 + 112),
      },
  };
}

__attribute__((always_inline)) static inline n8_sym_vectors
load_n8_sym_vectors() {
  return {
      {
          vld1q_s8(f1_n8 + 16),
          vld1q_s8(f1_n8 + 32),
          vld1q_s8(f1_n8 + 48),
      },
      {
          vld1q_s8(f2_n8 + 16),
          vld1q_s8(f2_n8 + 32),
          vld1q_s8(f2_n8 + 48),
      },
      vld1q_s32(n8_sym_signs4),
      vld1q_u8(cmplx_pair_reverse_indices),
      vld1q_s32(cmplx_pair_signs),
  };
}

__attribute__((always_inline)) static inline n8_sym2_vectors
load_n8_sym2_vectors() {
  const int8_t *tables = n8_sym2_tables;
  const int8x16_t f1 = vld1q_s8(tables + 16);
  return {
      vreinterpretq_u8_s8(vld1q_s8(tables)),
      f1,
      vreinterpretq_s8_s32(vrev64q_s32(vreinterpretq_s32_s8(f1))),
      vld1q_s8(tables + 32),
      // vld1q_u8(h73_indices),
      // vld1q_u8(h15_indices),
  };
}

/* Interleave two real and two imaginary values. */
__attribute__((always_inline)) static inline int32x4_t
interleave_cmplx_s32(const int32x2_t real, const int32x2_t imag) {
  return vcombine_s32(vzip1_s32(real, imag), vzip2_s32(real, imag));
}

/**
 * Matmul by a four-point Hadamard.
 *
 * [[1  1  1  1] [x_0   [h_0
 *  [1 -1  1 -1]  x_1    h_1
 *  [1  1 -1 -1]  x_2  = h_2
 *  [1 -1 -1  1]] x_3]   h_3]
 *
 * @return {h02, h13}; h02={h0,h2}, h13={h1,h3}
 */
__attribute__((always_inline)) static inline hadamard32x2x2
hadamard32x4(const int32x4_t x) {
  const int32x2_t lo = vget_low_s32(x);
  const int32x2_t hi = vget_high_s32(x);
  const int32x2_t sum = vadd_s32(lo, hi);
  const int32x2_t diff = vsub_s32(lo, hi);
  const int32x2_t first = vtrn1_s32(sum, diff);
  const int32x2_t second = vtrn2_s32(sum, diff);
  return {vadd_s32(first, second), vsub_s32(first, second)};
}

__attribute__((always_inline)) static inline hadamard32x4x2
hadamard32x4(const int32x4_t x, const int32x4_t y) {
  const int32x4_t even = vuzp1q_s32(x, y);
  const int32x4_t odd = vuzp2q_s32(x, y);

  const int32x4_t sum = vaddq_s32(even, odd);
  const int32x4_t diff = vsubq_s32(even, odd);

  const int32x4_t first = vuzp1q_s32(sum, diff);
  const int32x4_t second = vuzp2q_s32(sum, diff);

  // {x_h0, y_h0, x_h1, y_h1}
  const int32x4_t h01 = vaddq_s32(first, second);
  // {x_h2, y_h2, x_h3, y_h3}
  const int32x4_t h23 = vsubq_s32(first, second);

  return {h01, h23};
}

/**
 * Complex multiply; generates a symmetric pair of rotations.
 *
 * @param x Twiddled real and imag inputs,
 *          [re(F)re(x), -im(F)re(x), re(F)im(x), -im(F)im(x)]
 * @return {z * exp(+iθ), z * exp(-iθ)}
 */
__attribute__((always_inline)) static inline int32x4_t
cmplx_conjugate_pair(const int32x4_t x, const uint8x16_t indices,
                     const int32x4_t cmplx_signs) {
  const int32x4_t reversed =
      vreinterpretq_s32_u8(vqtbl1q_u8(vreinterpretq_u8_s32(x), indices));
  const int32x4_t pair = vmlaq_s32(reversed, x, cmplx_signs);
  return vcombine_s32(vget_low_s32(pair), vrev64_s32(vget_high_s32(pair)));
}

__attribute__((always_inline)) static inline int32x4_t
_vdotq_s32(int32x4_t acc, int8x16_t _a, int8x16_t _b) {
#ifdef DFT1D_DOTPROD
  return vdotq_s32(acc, _a, _b);
#else
  int16x8_t prod_lo = vmull_s8(vget_low_s8(_a), vget_low_s8(_b));
  int16x8_t prod_hi = vmull_s8(vget_high_s8(_a), vget_high_s8(_b));
  int32x4_t pair_lo = vpaddlq_s16(prod_lo);
  int32x4_t pair_hi = vpaddlq_s16(prod_hi);

  int32x4_t dot = vpaddq_s32(pair_lo, pair_hi);
  return vaddq_s32(acc, dot);
#endif
}

/**
 * Complex dot product of two selected rows and inputs.
 * @return {re(X[K]), im(X[K]), re(X[L]), im(X[L])}
 */
template<int K, int L>
__attribute__((always_inline)) static inline int32x4_t
f4_cdot_s32(const int8x16_t input, const n8_dft_vectors &tables) {
  static_assert(K < 8 && L < 8, "Row index out of range.");
  const int32x4_t zeros = vdupq_n_s32(0);
  const int32x4_t row_k = vpaddq_s32(_vdotq_s32(zeros, tables.f1[K], input),
                                     _vdotq_s32(zeros, tables.f2[K], input));
  const int32x4_t row_l = vpaddq_s32(_vdotq_s32(zeros, tables.f1[L], input),
                                     _vdotq_s32(zeros, tables.f2[L], input));

  return vpaddq_s32(row_k, row_l);
}

/**
 * Complex dot product of one row and inputs;
 * conventionally used for twiddle row tail.
 * @return {re(X[K]), im(X[K])}
 */
template<int K>
__attribute__((always_inline)) static inline int32x2_t
f2_cdot_s32(const int8x16_t input, const n8_dft_vectors &tables) {
  static_assert(K < 8, "Row index out of range.");
  const int32x4_t zeros = vdupq_n_s32(0);
  const int32x4_t row_k = vpaddq_s32(_vdotq_s32(zeros, tables.f1[K], input),
                                     _vdotq_s32(zeros, tables.f2[K], input));

  return vget_low_s32(vpaddq_s32(row_k, zeros));
}
} // namespace dft1d_neon

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_v2_impl(const std::complex<int8_t> *in,
                        std::complex<int8_t> *out,
                        const dft1d_neon::n8_dft_vectors &tables) {

  const int8x8x2_t in_vld = vld2_s8(reinterpret_cast<const int8_t *>(in));
  const int8x16_t input = vcombine_s8(in_vld.val[0], in_vld.val[1]);

  const int32x4_t zeros32 = vdupq_n_s32(0);
  const int16x4_t zeros16 = vdup_n_s16(0);

  for (uint8_t k = 0; k < 8; k += 2) {
    int32x4_t acc1 =
        vpaddq_s32(dft1d_neon::_vdotq_s32(zeros32, tables.f1[k], input),
                   dft1d_neon::_vdotq_s32(zeros32, tables.f2[k], input));
    int32x4_t acc2 =
        vpaddq_s32(dft1d_neon::_vdotq_s32(zeros32, tables.f1[k + 1], input),
                   dft1d_neon::_vdotq_s32(zeros32, tables.f2[k + 1], input));

    // Round shift saturate
    const int16x4_t rounded = vqrshrn_n_s32(vpaddq_s32(acc1, acc2), 10);
    int32_t val = vget_lane_s32(
        vreinterpret_s32_s8(vqmovn_s16(vcombine_s16(rounded, zeros16))), 0);
    memcpy(static_cast<void *>(out + k), &val, 4);
  }
}

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_v4_impl(const std::complex<int8_t> *in,
                        std::complex<int8_t> *out,
                        const dft1d_neon::n8_dft_vectors &tables) {

  const int8x8x2_t in_vld = vld2_s8(reinterpret_cast<const int8_t *>(in));
  const int8x16_t input = vcombine_s8(in_vld.val[0], in_vld.val[1]);

  // Utilize 4 vector registers
  const int32x4_t X01 = dft1d_neon::f4_cdot_s32<0, 1>(input, tables);
  const int32x4_t X23 = dft1d_neon::f4_cdot_s32<2, 3>(input, tables);
  const int32x4_t X45 = dft1d_neon::f4_cdot_s32<4, 5>(input, tables);
  const int32x4_t X67 = dft1d_neon::f4_cdot_s32<6, 7>(input, tables);

  // Round shift saturate
  const int16x4_t out01 = vqrshrn_n_s32(X01, 10);
  const int16x4_t out23 = vqrshrn_n_s32(X23, 10);
  const int16x4_t out45 = vqrshrn_n_s32(X45, 10);
  const int16x4_t out67 = vqrshrn_n_s32(X67, 10);

  const int8x8_t out03 = vqmovn_s16(vcombine_s16(out01, out23));
  const int8x8_t out47 = vqmovn_s16(vcombine_s16(out45, out67));
  vst1q_s8(reinterpret_cast<int8_t *>(out), vcombine_s8(out03, out47));
}

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_sym_impl(const std::complex<int8_t> *in,
                         std::complex<int8_t> *out,
                         const dft1d_neon::n8_sym_vectors &tables) {

  const int8x8x2_t in_vld = vld2_s8(reinterpret_cast<const int8_t *>(in));
  const int8x16_t input = vcombine_s8(in_vld.val[0], in_vld.val[1]);

  // Row 4
  const int8x8x2_t lanes = vuzp_s8(vget_low_s8(input), vget_high_s8(input));
  const int16x8_t vpsub = vsubl_s8(lanes.val[0], lanes.val[1]);
  const int32x4_t vals4 = vpaddlq_s16(vpsub);
  const int32x2_t X4 =
      vshl_n_s32(vpadd_s32(vget_low_s32(vals4), vget_high_s32(vals4)), 7);

  // Row 0 of all ones * inputs
  const int16x8_t sum16 = vpaddlq_s8(input);
  const int32x4_t sum32 = vpaddlq_s16(sum16);
  const int32x2_t X0 =
      vshl_n_s32(vpadd_s32(vget_low_s32(sum32), vget_high_s32(sum32)), 7);

  const int32x4_t zeros = vdupq_n_s32(0);

#ifdef DFT1D_I8MM
  // Rows 1 and 7
  const int32x4_t X17 =
      dft1d_neon::cmplx_conjugate_pair(vmmlaq_s32(zeros, input, tables.f1[0]),
                                       tables.indices, tables.cmplx_signs);
  // Rows 2 and 6
  const int32x4_t X26 =
      dft1d_neon::cmplx_conjugate_pair(vmmlaq_s32(zeros, input, tables.f1[1]),
                                       tables.indices, tables.cmplx_signs);
  // Rows 3 and 5
  const int32x4_t X35 =
      dft1d_neon::cmplx_conjugate_pair(vmmlaq_s32(zeros, input, tables.f1[2]),
                                       tables.indices, tables.cmplx_signs);
#else
  // Rows 1 and 7
  const int32x4_t vals17 =
      vpaddq_s32(dft1d_neon::_vdotq_s32(zeros, tables.f1[0], input),
                 dft1d_neon::_vdotq_s32(zeros, tables.f2[0], input));
  const int32x4_t X17 = vpaddq_s32(vals17, vmulq_s32(vals17, tables.signs4));

  // Rows 2 and 6
  const int32x4_t vals26 =
      vpaddq_s32(dft1d_neon::_vdotq_s32(zeros, tables.f1[1], input),
                 dft1d_neon::_vdotq_s32(zeros, tables.f2[1], input));
  const int32x4_t X26 = vpaddq_s32(vals26, vmulq_s32(vals26, tables.signs4));

  // Rows 3 and 5
  const int32x4_t vals35 =
      vpaddq_s32(dft1d_neon::_vdotq_s32(zeros, tables.f1[2], input),
                 dft1d_neon::_vdotq_s32(zeros, tables.f2[2], input));
  const int32x4_t X35 = vpaddq_s32(vals35, vmulq_s32(vals35, tables.signs4));
#endif

  const int32x4_t X01 = vcombine_s32(X0, vget_low_s32(X17));
  const int32x4_t X23 = vcombine_s32(vget_low_s32(X26), vget_low_s32(X35));
  const int32x4_t X45 = vcombine_s32(X4, vget_high_s32(X35));
  const int32x4_t X67 = vcombine_s32(vget_high_s32(X26), vget_high_s32(X17));

  // Round shift saturate
  const int16x4_t out01 = vqrshrn_n_s32(X01, 10);
  const int16x4_t out23 = vqrshrn_n_s32(X23, 10);
  const int16x4_t out45 = vqrshrn_n_s32(X45, 10);
  const int16x4_t out67 = vqrshrn_n_s32(X67, 10);

  const int8x8_t out03 = vqmovn_s16(vcombine_s16(out01, out23));
  const int8x8_t out47 = vqmovn_s16(vcombine_s16(out45, out67));
  vst1q_s8(reinterpret_cast<int8_t *>(out), vcombine_s8(out03, out47));
}

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_sym2_impl(const std::complex<int8_t> *in,
                          std::complex<int8_t> *out,
                          const dft1d_neon::n8_sym2_vectors &tables) {

  const int8x16_t in_vld = vld1q_s8(reinterpret_cast<const int8_t *>(in));
  const int8x16_t input = vqtbl1q_s8(in_vld, tables.indices);

  // Row 0 of all ones * inputs
  const int16x8_t sum16 = vpaddlq_s8(input);
  const int32x4_t sum32 = vpaddlq_s16(sum16);
  const int32x2_t lo = vget_low_s32(sum32);
  const int32x2_t hi = vget_high_s32(sum32);
  const int32x2_t X0 = vshl_n_s32(vadd_s32(lo, hi), 7);

  // Row 4 of ones and negates
  const int32x2_t X4 = vshl_n_s32(vsub_s32(lo, hi), 7);
  const int32x4_t zeros = vdupq_n_s32(0);

  // Rows 1, 3, 5, 7
  const int32x4_t frxrfixi = dft1d_neon::_vdotq_s32(zeros, tables.f1, input);
  const int32x4_t fixrfrxi = dft1d_neon::_vdotq_s32(zeros, tables.f2, input);
  const dft1d_neon::hadamard32x2x2 real_h = dft1d_neon::hadamard32x4(frxrfixi);
  const dft1d_neon::hadamard32x2x2 imag_h = dft1d_neon::hadamard32x4(fixrfrxi);
  // const auto h = dft1d_neon::hadamard32x4(frxrfixi, fixrfrxi);

  /**
   * re(X1)=h1, re(X3)=h2, re(X5)=h3, re(X7)=h0;
   * im(X1)=h0, im(X3)=-h3, im(X5)=h2, im(X7)=-h1
   */
  const int32x4_t X73 =
      dft1d_neon::interleave_cmplx_s32(real_h.h02, vneg_s32(imag_h.h13));
  const int32x4_t X15 =
      dft1d_neon::interleave_cmplx_s32(real_h.h13, imag_h.h02);

  // Reversing adjacent y lanes makes its Hadamard produce
  // {i_h0, -i_h1, i_h2, -i_h3}.
  // const auto h = dft1d_neon::hadamard32x4(frxrfixi, vrev64q_s32(fixrfrxi));

  // const int8x16x2_t table = {{
  //     vreinterpretq_s8_s32(h.h01),
  //     vreinterpretq_s8_s32(h.h23),
  // }};

  // const int32x4_t X73 = vreinterpretq_s32_s8(vqtbl2q_s8(table, tables.h73));
  // const int32x4_t X15 = vreinterpretq_s32_s8(vqtbl2q_s8(table, tables.h15));

  // Rows 2, 6
  const int32x4_t X26 = dft1d_neon::_vdotq_s32(zeros, tables.f3, input);
  const int32x2_t X26_lo = vget_low_s32(X26);
  const int32x2_t X26_hi_rev = vrev64_s32(vget_high_s32(X26));
  const int32x2_t X2 = vadd_s32(X26_lo, X26_hi_rev);
  const int32x2_t X6 = vsub_s32(X26_lo, X26_hi_rev);

  const int32x4_t X01 = vcombine_s32(X0, vget_low_s32(X15));
  const int32x4_t X23 = vcombine_s32(X2, vget_high_s32(X73));
  const int32x4_t X45 = vcombine_s32(X4, vget_high_s32(X15));
  const int32x4_t X67 = vcombine_s32(X6, vget_low_s32(X73));

  // Round shift saturate
  const int16x8_t out03 = vqrshrn_high_n_s32(vqrshrn_n_s32(X01, 10), X23, 10);
  const int16x8_t out47 = vqrshrn_high_n_s32(vqrshrn_n_s32(X45, 10), X67, 10);
  const int8x16_t output = vqmovn_high_s16(vqmovn_s16(out03), out47);
  vst1q_s8(reinterpret_cast<int8_t *>(out), output);
}

/* Execute routines. */
__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_v2(const std::complex<int8_t> *in, std::complex<int8_t> *out) {

  static_assert(sizeof(std::complex<int8_t>) == 2 * sizeof(int8_t),
                "This kernel requires interleaved, byte-sized complex data.");
  const dft1d_neon::n8_dft_vectors tables = dft1d_neon::load_n8_dft_vectors();
  c2c_n8_vdft_s32_v2_impl(in, out, tables);
}

static inline void c2c_n8_vdft_s32_v2_batch(const std::complex<int8_t> *in,
                                            std::complex<int8_t> *out,
                                            const std::size_t howmany) {
  const dft1d_neon::n8_dft_vectors tables = dft1d_neon::load_n8_dft_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_v2_impl(in + 8 * hm, out + 8 * hm, tables);
  }
}

static inline void c2c_n8_vdft_s32_v2_exec(const std::complex<int8_t> *in,
                                           std::complex<int8_t> *out,
                                           const std::size_t howmany,
                                           const std::size_t idist,
                                           const std::size_t odist) {
  const dft1d_neon::n8_dft_vectors tables = dft1d_neon::load_n8_dft_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_v2_impl(in + idist * hm, out + odist * hm, tables);
  }
}

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_v4(const std::complex<int8_t> *in, std::complex<int8_t> *out) {

  static_assert(sizeof(std::complex<int8_t>) == 2 * sizeof(int8_t),
                "This kernel requires interleaved, byte-sized complex data.");
  const dft1d_neon::n8_dft_vectors tables = dft1d_neon::load_n8_dft_vectors();
  c2c_n8_vdft_s32_v4_impl(in, out, tables);
}

static inline void c2c_n8_vdft_s32_v4_batch(const std::complex<int8_t> *in,
                                            std::complex<int8_t> *out,
                                            const std::size_t howmany) {
  const dft1d_neon::n8_dft_vectors tables = dft1d_neon::load_n8_dft_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_v4_impl(in + 8 * hm, out + 8 * hm, tables);
  }
}

static inline void c2c_n8_vdft_s32_v4_exec(const std::complex<int8_t> *in,
                                           std::complex<int8_t> *out,
                                           const std::size_t howmany,
                                           const std::size_t idist,
                                           const std::size_t odist) {
  const dft1d_neon::n8_dft_vectors tables = dft1d_neon::load_n8_dft_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_v4_impl(in + idist * hm, out + odist * hm, tables);
  }
}

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_sym(const std::complex<int8_t> *in, std::complex<int8_t> *out) {

  static_assert(sizeof(std::complex<int8_t>) == 2 * sizeof(int8_t),
                "This kernel requires interleaved, byte-sized complex data.");
  const dft1d_neon::n8_sym_vectors tables = dft1d_neon::load_n8_sym_vectors();
  c2c_n8_vdft_s32_sym_impl(in, out, tables);
}

static inline void c2c_n8_vdft_s32_sym_batch(const std::complex<int8_t> *in,
                                             std::complex<int8_t> *out,
                                             const std::size_t howmany) {
  const dft1d_neon::n8_sym_vectors tables = dft1d_neon::load_n8_sym_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_sym_impl(in + 8 * hm, out + 8 * hm, tables);
  }
}

static inline void c2c_n8_vdft_s32_sym_exec(const std::complex<int8_t> *in,
                                            std::complex<int8_t> *out,
                                            const std::size_t howmany,
                                            const std::size_t idist,
                                            const std::size_t odist) {
  const dft1d_neon::n8_sym_vectors tables = dft1d_neon::load_n8_sym_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_sym_impl(in + idist * hm, out + odist * hm, tables);
  }
}

__attribute__((always_inline)) static inline void
c2c_n8_vdft_s32_sym2(const std::complex<int8_t> *in,
                     std::complex<int8_t> *out) {

  static_assert(sizeof(std::complex<int8_t>) == 2 * sizeof(int8_t),
                "This kernel requires interleaved, byte-sized complex data.");
  const dft1d_neon::n8_sym2_vectors tables = dft1d_neon::load_n8_sym2_vectors();
  c2c_n8_vdft_s32_sym2_impl(in, out, tables);
}

static inline void c2c_n8_vdft_s32_sym2_batch(const std::complex<int8_t> *in,
                                              std::complex<int8_t> *out,
                                              const std::size_t howmany) {
  const dft1d_neon::n8_sym2_vectors tables = dft1d_neon::load_n8_sym2_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_sym2_impl(in + 8 * hm, out + 8 * hm, tables);
  }
}

static inline void c2c_n8_vdft_s32_sym2_exec(const std::complex<int8_t> *in,
                                             std::complex<int8_t> *out,
                                             const std::size_t howmany,
                                             const std::size_t idist,
                                             const std::size_t odist) {
  const dft1d_neon::n8_sym2_vectors tables = dft1d_neon::load_n8_sym2_vectors();
  for (std::size_t hm = 0; hm < howmany; ++hm) {
    c2c_n8_vdft_s32_sym2_impl(in + idist * hm, out + odist * hm, tables);
  }
}
