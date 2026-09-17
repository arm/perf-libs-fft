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

ARM_LOCALLY_STREAMING_NEW_ZA void
transpose_t_to_u_sme2(const std::complex<float> *buffer, std::complex<float> *Y,
                      const int64_t ostride, const int64_t howmany,
                      const int64_t n) {
  assert(svcntw() == 16);

  const int64_t n_multiple_32 = n & ~31;
  const int64_t howmany_multiple_16 = howmany & ~15;
  const auto *buffer_f = reinterpret_cast<const float32_t *>(buffer);
  auto *Y_f = reinterpret_cast<float32_t *>(Y);
  const int64_t f_ostride = ostride * 2;
  constexpr int64_t f_odist = 2;

  const int64_t tail_howmany = howmany - howmany_multiple_16;
  const svbool_t tail_pg = svwhilelt_b32_s64(0, tail_howmany);

  for (int64_t point = 0; point < n_multiple_32; point += 32) {
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_f + hm * n * 2 + point * 2;
      auto *out = Y_f + hm * f_odist + point * f_ostride;

      load_t_layout_rows_to_za<true>(in, n, 16);
      store_za_columns_to_output<4>(svptrue_b32(), out, f_ostride,
                                    8 * f_ostride);
    }

    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_f + howmany_multiple_16 * n * 2 + point * 2;
      auto *out = Y_f + howmany_multiple_16 * f_odist + point * f_ostride;

      load_t_layout_rows_to_za<true>(in, n, tail_howmany);
      store_za_columns_to_output<4>(tail_pg, out, f_ostride, 8 * f_ostride);
    }
  }

  if (n_multiple_32 < n) {
    const int64_t point = n_multiple_32;
    for (int64_t hm = 0; hm < howmany_multiple_16; hm += 16) {
      const auto *in = buffer_f + hm * n * 2 + point * 2;
      auto *out = Y_f + hm * f_odist + point * f_ostride;

      load_t_layout_rows_to_za<false>(in, n, 16);
      store_za_columns_to_output<2>(svptrue_b32(), out, f_ostride,
                                    8 * f_ostride);
    }

    if (howmany_multiple_16 < howmany) {
      const auto *in = buffer_f + howmany_multiple_16 * n * 2 + point * 2;
      auto *out = Y_f + howmany_multiple_16 * f_odist + point * f_ostride;

      load_t_layout_rows_to_za<false>(in, n, tail_howmany);
      store_za_columns_to_output<2>(tail_pg, out, f_ostride, 8 * f_ostride);
    }
  }
}

} // namespace

void plfft_16x_cccn_tu_sme2(const std::complex<float> *X,
                            std::complex<float> *Y, int64_t /*istride == 1*/,
                            int64_t ostride, const std::complex<float> *W_n1,
                            const std::complex<float> *W_n2,
                            const std::complex<float> *tw, int64_t howmany,
                            int64_t idist, int64_t /*odist == 1*/,
                            const int64_t n) {
  auto *buffer =
      get_memory<std::complex<float>>(buffer_name::sme2_direct, n * howmany);
  plfft_16x_cccn_tt_sme2(X, buffer, 1, 1, W_n1, W_n2, tw, howmany, idist, n, n);
  transpose_t_to_u_sme2(buffer, Y, ostride, howmany, n);
}

} // namespace plfft
