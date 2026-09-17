/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
/*
 * X = Fx for N = 8, scalar dot product kernel
 *
 * Q0.7 inputs -> Q0.14 intermediate -> Q0.7 outputs
 */
#pragma once

#include <complex>
#include <stddef.h>
#include <stdint.h>

#define DFT_SIZE 8u
#define BITS_PER_ENTRY 3u
#define ROW_BYTES 3u

/**
 * Symbol encoding:
 *
 *   0 ->  0
 *   1 ->  1
 *   2 -> -1
 *   3 ->  1/sqrt(2)
 *   4 -> -1/sqrt(2)
 */
enum {
  DFT_ZERO = 0,
  DFT_ONE = 1,
  DFT_NEG_ONE = 2,
  DFT_INV_SQRT2 = 3,
  DFT_NEG_INV_SQRT2 = 4
};

/* Q0.14 representation of 1 and 1/sqrt(2) */
#define ONE_Q0_14 16384
#define INV_SQRT2_Q0_14 11585

/**
 * Pack 8x 3-bit entries into 3 bytes
 *
 * Entry 0 occupies bits 0..2
 * Entry 1 occupies bits 3..5
 * ...
 * Entry 7 occupies bits 21..23
 */
#define PACK_ROW(a, b, c, d, e, f, g, h)                                       \
  {(uint8_t)(((uint32_t)(a)) | ((uint32_t)(b) << 3) | ((uint32_t)(c) << 6)),   \
   (uint8_t)(((uint32_t)(c) >> 2) | ((uint32_t)(d) << 1) |                     \
             ((uint32_t)(e) << 4) | ((uint32_t)(f) << 7)),                     \
   (uint8_t)(((uint32_t)(f) >> 1) | ((uint32_t)(g) << 2) |                     \
             ((uint32_t)(h) << 5))}

/* Real part of F8 */
static const uint8_t f8_real_row_0[ROW_BYTES] = PACK_ROW(
    DFT_ONE, DFT_ONE, DFT_ONE, DFT_ONE, DFT_ONE, DFT_ONE, DFT_ONE, DFT_ONE);

static const uint8_t f8_real_row_1[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_INV_SQRT2, DFT_ZERO, DFT_NEG_INV_SQRT2, DFT_NEG_ONE,
             DFT_NEG_INV_SQRT2, DFT_ZERO, DFT_INV_SQRT2);

static const uint8_t f8_real_row_2[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_ZERO, DFT_NEG_ONE, DFT_ZERO, DFT_ONE, DFT_ZERO,
             DFT_NEG_ONE, DFT_ZERO);

static const uint8_t f8_real_row_3[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_NEG_INV_SQRT2, DFT_ZERO, DFT_INV_SQRT2, DFT_NEG_ONE,
             DFT_INV_SQRT2, DFT_ZERO, DFT_NEG_INV_SQRT2);

static const uint8_t f8_real_row_4[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_NEG_ONE, DFT_ONE, DFT_NEG_ONE, DFT_ONE, DFT_NEG_ONE,
             DFT_ONE, DFT_NEG_ONE);

static const uint8_t f8_real_row_5[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_NEG_INV_SQRT2, DFT_ZERO, DFT_INV_SQRT2, DFT_NEG_ONE,
             DFT_INV_SQRT2, DFT_ZERO, DFT_NEG_INV_SQRT2);

static const uint8_t f8_real_row_6[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_ZERO, DFT_NEG_ONE, DFT_ZERO, DFT_ONE, DFT_ZERO,
             DFT_NEG_ONE, DFT_ZERO);

static const uint8_t f8_real_row_7[ROW_BYTES] =
    PACK_ROW(DFT_ONE, DFT_INV_SQRT2, DFT_ZERO, DFT_NEG_INV_SQRT2, DFT_NEG_ONE,
             DFT_NEG_INV_SQRT2, DFT_ZERO, DFT_INV_SQRT2);

/* Imaginary part of F8 */
static const uint8_t f8_imag_row_0[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_ZERO, DFT_ZERO, DFT_ZERO, DFT_ZERO, DFT_ZERO,
             DFT_ZERO, DFT_ZERO);

static const uint8_t f8_imag_row_1[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_NEG_INV_SQRT2, DFT_NEG_ONE, DFT_NEG_INV_SQRT2,
             DFT_ZERO, DFT_INV_SQRT2, DFT_ONE, DFT_INV_SQRT2);

static const uint8_t f8_imag_row_2[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_NEG_ONE, DFT_ZERO, DFT_ONE, DFT_ZERO, DFT_NEG_ONE,
             DFT_ZERO, DFT_ONE);

static const uint8_t f8_imag_row_3[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_NEG_INV_SQRT2, DFT_ONE, DFT_NEG_INV_SQRT2, DFT_ZERO,
             DFT_INV_SQRT2, DFT_NEG_ONE, DFT_INV_SQRT2);

static const uint8_t f8_imag_row_4[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_ZERO, DFT_ZERO, DFT_ZERO, DFT_ZERO, DFT_ZERO,
             DFT_ZERO, DFT_ZERO);

static const uint8_t f8_imag_row_5[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_INV_SQRT2, DFT_NEG_ONE, DFT_INV_SQRT2, DFT_ZERO,
             DFT_NEG_INV_SQRT2, DFT_ONE, DFT_NEG_INV_SQRT2);

static const uint8_t f8_imag_row_6[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_ONE, DFT_ZERO, DFT_NEG_ONE, DFT_ZERO, DFT_ONE,
             DFT_ZERO, DFT_NEG_ONE);

static const uint8_t f8_imag_row_7[ROW_BYTES] =
    PACK_ROW(DFT_ZERO, DFT_INV_SQRT2, DFT_ONE, DFT_INV_SQRT2, DFT_ZERO,
             DFT_NEG_INV_SQRT2, DFT_NEG_ONE, DFT_NEG_INV_SQRT2);

/* Row lookup tables */
static const uint8_t *const f8_real_rows[DFT_SIZE] = {
    f8_real_row_0, f8_real_row_1, f8_real_row_2, f8_real_row_3,
    f8_real_row_4, f8_real_row_5, f8_real_row_6, f8_real_row_7};

static const uint8_t *const f8_imag_rows[DFT_SIZE] = {
    f8_imag_row_0, f8_imag_row_1, f8_imag_row_2, f8_imag_row_3,
    f8_imag_row_4, f8_imag_row_5, f8_imag_row_6, f8_imag_row_7};

/* Extract one 3-bit symbol from a packed row */
static uint8_t dft_get_symbol(const uint8_t row[ROW_BYTES], size_t column) {

  uint32_t packed;
  if (column >= DFT_SIZE) {
    return DFT_ZERO;
  }
  packed =
      ((uint32_t)row[0]) | ((uint32_t)row[1] << 8) | ((uint32_t)row[2] << 16);

  return (uint8_t)((packed >> (BITS_PER_ENTRY * column)) & 0x07u);
}

/* Multiply by twiddle cases */
static int32_t mul_twiddle_factors(int8_t x, uint8_t symbol) {

  switch (symbol) {
  case DFT_NEG_ONE:
    return -(int32_t)x * ONE_Q0_14;

  case DFT_NEG_INV_SQRT2:
    return -(int32_t)x * INV_SQRT2_Q0_14;

  case DFT_ZERO:
    return 0;

  case DFT_INV_SQRT2:
    return (int32_t)x * INV_SQRT2_Q0_14;

  case DFT_ONE:
    return (int32_t)x * ONE_Q0_14;

  default:
    return 0;
  }
}

/**
 * Round shift saturate.
 *
 * Round to nearest, with ties away from zero
 * Shift by Q0.x + 3 bits
 */
static int8_t rss_s8(int32_t value, unsigned x) {

  const int32_t half = (int32_t)1 << (x - 1);
  value += (value >= 0) ? half : half - 1;
  value >>= x;

  if (value > INT8_MAX)
    return INT8_MAX;
  if (value < INT8_MIN)
    return INT8_MIN;
  return (int8_t)(value);
}

/**
 * Scalar dot product of N=8 times out[row] <- dot(f[row], in[row])
 *
 * @param in  Complex valued input data, in.data()
 * @param out Complex valued output data, out.data()
 */
static void c2c_n8_dft_s32(const std::complex<int8_t> *in,
                           std::complex<int8_t> *out) {

  size_t k;
  size_t l;

  for (k = 0; k < 8; ++k) {

    int32_t frxr = 0;
    int32_t fixi = 0;
    int32_t frxi = 0;
    int32_t fixr = 0;

    for (l = 0; l < 8; ++l) {

      const auto &value = in[l];
      frxr += mul_twiddle_factors(+value.real(),
                                  dft_get_symbol(f8_real_rows[k], l));
      fixi += mul_twiddle_factors(+value.imag(),
                                  dft_get_symbol(f8_imag_rows[k], l));
      frxi += mul_twiddle_factors(+value.imag(),
                                  dft_get_symbol(f8_real_rows[k], l));
      fixr += mul_twiddle_factors(+value.real(),
                                  dft_get_symbol(f8_imag_rows[k], l));
    }

    int32_t real = frxr - fixi;
    int32_t imag = frxi + fixr;

    out[k] = {rss_s8(real, 17), rss_s8(imag, 17)};
  }
}
