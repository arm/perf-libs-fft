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

ARM_LOCALLY_STREAMING_NEW_ZA void
transpose_t_to_u_sme2(const complex_half *buffer, complex_half *Y,
                      const int64_t ostride, const int64_t howmany,
                      const int64_t n) {
  assert(svcntw() == 16);
  assert((n % 16) == 0);

  const int64_t n_multiple_64 = n & ~63;
  const int64_t howmany_multiple_16 = howmany & ~15;
  const auto *buffer_u32 = reinterpret_cast<const uint32_t *>(buffer);
  auto *Y_u32 = reinterpret_cast<uint32_t *>(Y);

  const int64_t tail_howmany = howmany - howmany_multiple_16;
  const svbool_t tail_pg = svwhilelt_b32_s64(0, tail_howmany);

  for (int64_t point = 0; point < n_multiple_64; point += 64) {
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_u32 + hm * n + point;
      auto *out = Y_u32 + hm + point * ostride;

      load_t_layout_complex_half_rows_to_za<4>(in, n, 16);
      store_za_columns_to_output_u32<4>(svptrue_b32(), out, ostride,
                                        16 * ostride);
    }

    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_u32 + howmany_multiple_16 * n + point;
      auto *out = Y_u32 + howmany_multiple_16 + point * ostride;

      load_t_layout_complex_half_rows_to_za<4>(in, n, tail_howmany);
      store_za_columns_to_output_u32<4>(tail_pg, out, ostride, 16 * ostride);
    }
  }

  int64_t remaining_n = n - n_multiple_64;
  const int64_t n_multiple_32 = remaining_n & ~31;
  if (n_multiple_32 > 0) {
    const int64_t point = n_multiple_64;
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_u32 + hm * n + point;
      auto *out = Y_u32 + hm + point * ostride;

      load_t_layout_complex_half_rows_to_za<2>(in, n, 16);
      store_za_columns_to_output_u32<2>(svptrue_b32(), out, ostride,
                                        16 * ostride);
    }

    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_u32 + howmany_multiple_16 * n + point;
      auto *out = Y_u32 + howmany_multiple_16 + point * ostride;

      load_t_layout_complex_half_rows_to_za<2>(in, n, tail_howmany);
      store_za_columns_to_output_u32<2>(tail_pg, out, ostride, 16 * ostride);
    }
  }

  remaining_n -= n_multiple_32;
  if (remaining_n > 0) {
    const int64_t point = n_multiple_64 + n_multiple_32;
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_u32 + hm * n + point;
      auto *out = Y_u32 + hm + point * ostride;

      load_t_layout_complex_half_rows_to_za<1>(in, n, 16);
      store_za_columns_to_output_u32<1>(svptrue_b32(), out, ostride,
                                        16 * ostride);
    }

    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_u32 + howmany_multiple_16 * n + point;
      auto *out = Y_u32 + howmany_multiple_16 + point * ostride;

      load_t_layout_complex_half_rows_to_za<1>(in, n, tail_howmany);
      store_za_columns_to_output_u32<1>(tail_pg, out, ostride, 16 * ostride);
    }
  }
}

} // namespace

void plfft_16x_jjjn_tu_sme2(const complex_half *X, complex_half *Y,
                            int64_t /*istride == 1*/, int64_t ostride,
                            const complex_half *W_n1, const complex_half *W_n2,
                            const complex_half *tw, int64_t howmany,
                            int64_t idist, int64_t /*odist == 1*/,
                            const int64_t n) {
  auto *buffer =
      get_memory<complex_half>(buffer_name::sme2_direct, n * howmany);
  plfft_16x_jjjn_tt_sme2(X, buffer, 1, 1, W_n1, W_n2, tw, howmany, idist, n, n);
  transpose_t_to_u_sme2(buffer, Y, ostride, howmany, n);
}

} // namespace plfft
