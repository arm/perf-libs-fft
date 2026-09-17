/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "fft_buffers.hpp"
#include "kernels.hpp"
#include "plfft_assert.hpp"
#include "plfft_attrs.hpp"
#include "transpose_sme2_f32.hpp"

#include <arm_sme.h>
#include <complex>

namespace plfft {

namespace {

template<int TileCount>
PLFFT_ALWAYS_INLINE void transpose_u_to_t_block(
    const float32_t *buffer, float32_t *out, const int64_t point,
    const int64_t howmany, const int64_t odist, const svbool_t load_pg,
    const bool full_howmany_block,
    const int64_t howmany_tail) __arm_streaming __arm_inout("za") {
  const int64_t f_ostride = 2;
  const int64_t f_odist = odist * 2;

  load_u_layout_points_to_za<TileCount>(load_pg, buffer, howmany, point);
  if (full_howmany_block) {
    store_za_columns_to_output<TileCount>(svptrue_b32(), out, f_odist,
                                          16 * f_ostride);
  } else {
    store_za_tail_columns_to_output<TileCount>(out, f_odist, 16 * f_ostride,
                                               howmany_tail);
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA void
transpose_u_to_t_sme2(const std::complex<float> *buffer, std::complex<float> *Y,
                      const int64_t howmany, const int64_t odist,
                      const int64_t n) {
  assert(svcntw() == 16);

  const int64_t howmany_multiple_8 = howmany & ~7;
  const int64_t n_multiple_64 = n & ~63;
  const int64_t f_odist = odist * 2;
  const auto *buffer_f = reinterpret_cast<const float32_t *>(buffer);
  auto *Y_f = reinterpret_cast<float32_t *>(Y);
  const svbool_t full_pg = svptrue_b32();

  const int64_t tail_howmany = howmany - howmany_multiple_8;
  const svbool_t tail_pg = svwhilelt_b32_s64(0, 2 * tail_howmany);

  for (int64_t point = 0; point < n_multiple_64; point += 64) {
    for (int64_t hm = 0; hm < howmany_multiple_8; hm += 8) {
      const auto *in = buffer_f + hm * 2;
      auto *out = Y_f + hm * f_odist + point * 2;
      transpose_u_to_t_block<4>(in, out, point, howmany, odist, full_pg, true,
                                0);
    }
    if (howmany_multiple_8 < howmany) {
      const auto *in = buffer_f + howmany_multiple_8 * 2;
      auto *out = Y_f + howmany_multiple_8 * f_odist + point * 2;
      transpose_u_to_t_block<4>(in, out, point, howmany, odist, tail_pg, false,
                                tail_howmany);
    }
  }

  int64_t remaining_n = n - n_multiple_64;
  const int64_t n_multiple_32 = remaining_n & ~31;
  if (n_multiple_32 > 0) {
    const int64_t point = n_multiple_64;
    for (int64_t hm = 0; hm < howmany_multiple_8; hm += 8) {
      const auto *in = buffer_f + hm * 2;
      auto *out = Y_f + hm * f_odist + point * 2;
      transpose_u_to_t_block<2>(in, out, point, howmany, odist, full_pg, true,
                                0);
    }
    if (howmany_multiple_8 < howmany) {
      const auto *in = buffer_f + howmany_multiple_8 * 2;
      auto *out = Y_f + howmany_multiple_8 * f_odist + point * 2;
      transpose_u_to_t_block<2>(in, out, point, howmany, odist, tail_pg, false,
                                tail_howmany);
    }
  }

  remaining_n -= n_multiple_32;
  if (remaining_n > 0) {
    const int64_t point = n_multiple_64 + n_multiple_32;
    for (int64_t hm = 0; hm < howmany_multiple_8; hm += 8) {
      const auto *in = buffer_f + hm * 2;
      auto *out = Y_f + hm * f_odist + point * 2;
      transpose_u_to_t_block<1>(in, out, point, howmany, odist, full_pg, true,
                                0);
    }
    if (howmany_multiple_8 < howmany) {
      const auto *in = buffer_f + howmany_multiple_8 * 2;
      auto *out = Y_f + howmany_multiple_8 * f_odist + point * 2;
      transpose_u_to_t_block<1>(in, out, point, howmany, odist, tail_pg, false,
                                tail_howmany);
    }
  }
}

} // namespace

void plfft_16x_cccn_ut_sme2(
    const std::complex<float> *X, std::complex<float> *Y, int64_t istride,
    int64_t /*ostride == 1*/, const std::complex<float> *W_n1,
    const std::complex<float> *W_n2, const std::complex<float> *tw,
    int64_t howmany, int64_t /*idist == 1*/, int64_t odist, const int64_t n) {
  auto *buffer =
      get_memory<std::complex<float>>(buffer_name::sme2_direct, n * howmany);
  plfft_16x_cccn_uu_sme2(X, buffer, istride, howmany, W_n1, W_n2, tw, howmany,
                         1, 1, n);
  transpose_u_to_t_sme2(buffer, Y, howmany, odist, n);
}

} // namespace plfft
