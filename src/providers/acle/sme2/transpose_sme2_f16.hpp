/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_attrs.hpp"

#include <arm_sme.h>
#include <cstdint>

namespace plfft {

template<int TileCount>
__attribute__((noinline)) void load_t_layout_complex_half_rows_to_za(
    const uint32_t *in, const int64_t n,
    const int64_t rows) __arm_streaming __arm_out("za") {
  const svbool_t pg = svptrue_b32();
  for (int32_t slice = 0; slice < rows; ++slice) {
    svld1_hor_za32(0, slice, pg, in + slice * n);
    if constexpr (TileCount >= 2) {
      svld1_hor_za32(1, slice, pg, in + slice * n + 16);
    }
    if constexpr (TileCount >= 3) {
      svld1_hor_za32(2, slice, pg, in + slice * n + 32);
    }
    if constexpr (TileCount >= 4) {
      svld1_hor_za32(3, slice, pg, in + slice * n + 48);
    }
  }
}

template<int TileCount>
__attribute__((noinline)) void load_u_layout_complex_half_points_to_za(
    const svbool_t pg, const uint32_t *in, const int64_t howmany,
    const int64_t point) __arm_streaming __arm_out("za") {
  for (int32_t slice = 0; slice < 16; ++slice) {
    svld1_hor_za32(0, slice, pg, in + (point + slice) * howmany);
    if constexpr (TileCount >= 2) {
      svld1_hor_za32(1, slice, pg, in + (point + 16 + slice) * howmany);
    }
    if constexpr (TileCount >= 3) {
      svld1_hor_za32(2, slice, pg, in + (point + 32 + slice) * howmany);
    }
    if constexpr (TileCount >= 4) {
      svld1_hor_za32(3, slice, pg, in + (point + 48 + slice) * howmany);
    }
  }
}

PLFFT_ALWAYS_INLINE void
store_vertical_tile_u32_vg4(const svbool_t pg, uint32_t *base,
                            const int64_t column_stride,
                            const int64_t tile_stride, const int64_t tile,
                            const svuint32x4_t data) __arm_streaming {
  uint32_t *tile_base = base + tile * tile_stride;
  svst1_u32(pg, tile_base, svget4(data, 0));
  svst1_u32(pg, tile_base + column_stride, svget4(data, 1));
  svst1_u32(pg, tile_base + 2 * column_stride, svget4(data, 2));
  svst1_u32(pg, tile_base + 3 * column_stride, svget4(data, 3));
}

PLFFT_ALWAYS_INLINE void
store_vertical_tile_u32(const svbool_t pg, uint32_t *base,
                        const int64_t tile_stride, const int64_t tile,
                        const svuint32_t data) __arm_streaming {
  svst1_u32(pg, base + tile * tile_stride, data);
}

template<int TileCount>
__attribute__((noinline)) void store_za_columns_to_output_u32(
    const svbool_t pg, uint32_t *out, const int64_t column_stride,
    const int64_t tile_stride) __arm_streaming __arm_in("za") {
  for (int32_t slice = 0; slice < 16; slice += 4) {
    uint32_t *slice_out = out + slice * column_stride;
    store_vertical_tile_u32_vg4(pg, slice_out, column_stride, tile_stride, 0,
                                svread_ver_za32_u32_vg4(0, slice));
    if constexpr (TileCount >= 2) {
      store_vertical_tile_u32_vg4(pg, slice_out, column_stride, tile_stride, 1,
                                  svread_ver_za32_u32_vg4(1, slice));
    }
    if constexpr (TileCount >= 3) {
      store_vertical_tile_u32_vg4(pg, slice_out, column_stride, tile_stride, 2,
                                  svread_ver_za32_u32_vg4(2, slice));
    }
    if constexpr (TileCount >= 4) {
      store_vertical_tile_u32_vg4(pg, slice_out, column_stride, tile_stride, 3,
                                  svread_ver_za32_u32_vg4(3, slice));
    }
  }
}

template<int TileCount>
__attribute__((noinline)) void store_za_tail_columns_to_output_u32(
    uint32_t *out, const int64_t column_stride, const int64_t tile_stride,
    const int64_t columns) __arm_streaming __arm_in("za") {
  const svbool_t pg = svptrue_b32();
  const svuint32_t inactive = svdup_n_u32(0);

  for (int32_t slice = 0; slice < columns; ++slice) {
    uint32_t *slice_out = out + slice * column_stride;
    store_vertical_tile_u32(pg, slice_out, tile_stride, 0,
                            svread_ver_za32_u32_m(inactive, pg, 0, slice));
    if constexpr (TileCount >= 2) {
      store_vertical_tile_u32(pg, slice_out, tile_stride, 1,
                              svread_ver_za32_u32_m(inactive, pg, 1, slice));
    }
    if constexpr (TileCount >= 3) {
      store_vertical_tile_u32(pg, slice_out, tile_stride, 2,
                              svread_ver_za32_u32_m(inactive, pg, 2, slice));
    }
    if constexpr (TileCount >= 4) {
      store_vertical_tile_u32(pg, slice_out, tile_stride, 3,
                              svread_ver_za32_u32_m(inactive, pg, 3, slice));
    }
  }
}

} // namespace plfft
