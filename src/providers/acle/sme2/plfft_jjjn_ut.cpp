/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "fft_buffers.hpp"
#include "kernels.hpp"
#include "plfft_attrs.hpp"
#include "transpose_sme2_f16.hpp"

#include <arm_sme.h>
#include <cassert>
#include <cstdint>

namespace plfft {

namespace {

template<int TileCount>
PLFFT_ALWAYS_INLINE void transpose_u_to_t_block(
    const uint32_t *buffer, uint32_t *out, const int64_t point,
    const int64_t howmany, const int64_t odist, const svbool_t load_pg,
    const bool full_howmany_block,
    const int64_t howmany_tail) __arm_streaming __arm_inout("za") {
  load_u_layout_complex_half_points_to_za<TileCount>(load_pg, buffer, howmany,
                                                     point);
  if (full_howmany_block) {
    store_za_columns_to_output_u32<TileCount>(svptrue_b32(), out, odist, 16);
  } else {
    store_za_tail_columns_to_output_u32<TileCount>(out, odist, 16,
                                                   howmany_tail);
  }
}

ARM_LOCALLY_STREAMING_NEW_ZA void
transpose_u_to_t_sme2(const complex_half *buffer, complex_half *Y,
                      const int64_t howmany, const int64_t odist,
                      const int64_t n) {
  assert(svcntw() == 16);
  assert(n % 16 == 0);

  const int64_t howmany_multiple_16 = howmany & ~15;
  const int64_t n_multiple_64 = n & ~63;
  const auto *buffer_u32 = reinterpret_cast<const uint32_t *>(buffer);
  auto *Y_u32 = reinterpret_cast<uint32_t *>(Y);
  const svbool_t pt = svptrue_b32();

  const int64_t tail_howmany = howmany - howmany_multiple_16;
  const svbool_t tail_pg = svwhilelt_b32_s64(0, tail_howmany);

  for (int64_t point = 0; point < n_multiple_64; point += 64) {
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_u32 + hm;
      auto *out = Y_u32 + hm * odist + point;
      transpose_u_to_t_block<4>(in, out, point, howmany, odist, pt, true, 0);
    }
    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_u32 + howmany_multiple_16;
      auto *out = Y_u32 + howmany_multiple_16 * odist + point;
      transpose_u_to_t_block<4>(in, out, point, howmany, odist, tail_pg, false,
                                tail_howmany);
    }
  }

  int64_t remaining_n = n - n_multiple_64;
  const int64_t n_multiple_32 = remaining_n & ~31;
  if (n_multiple_32 > 0) {
    const int64_t point = n_multiple_64;
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_u32 + hm;
      auto *out = Y_u32 + hm * odist + point;
      transpose_u_to_t_block<2>(in, out, point, howmany, odist, pt, true, 0);
    }
    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_u32 + howmany_multiple_16;
      auto *out = Y_u32 + howmany_multiple_16 * odist + point;
      transpose_u_to_t_block<2>(in, out, point, howmany, odist, tail_pg, false,
                                tail_howmany);
    }
  }

  remaining_n -= n_multiple_32;
  if (remaining_n > 0) {
    const int64_t point = n_multiple_64 + n_multiple_32;
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_u32 + hm;
      auto *out = Y_u32 + hm * odist + point;
      transpose_u_to_t_block<1>(in, out, point, howmany, odist, pt, true, 0);
    }
    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_u32 + howmany_multiple_16;
      auto *out = Y_u32 + howmany_multiple_16 * odist + point;
      transpose_u_to_t_block<1>(in, out, point, howmany, odist, tail_pg, false,
                                tail_howmany);
    }
  }
}

} // namespace

void plfft_16x_jjjn_ut_sme2(const complex_half *X, complex_half *Y,
                            int64_t istride, int64_t /*ostride == 1*/,
                            const complex_half *W_n1, const complex_half *W_n2,
                            const complex_half *tw, int64_t howmany,
                            int64_t /*idist == 1*/, int64_t odist,
                            const int64_t n) {
  auto *buffer =
      get_memory<complex_half>(buffer_name::sme2_direct, n * howmany);
  plfft_16x_jjjn_uu_sme2(X, buffer, istride, howmany, W_n1, W_n2, tw, howmany,
                         1, 1, n);
  transpose_u_to_t_sme2(buffer, Y, howmany, odist, n);
}

} // namespace plfft
