/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_attrs.hpp"

#include <arm_sme.h>

namespace plfft {

template<bool UnrollTwo>
__attribute__((noinline)) void
load_t_layout_rows_to_za(const float32_t *in, const int64_t n,
                         const int64_t rows) __arm_streaming __arm_out("za") {
  const svbool_t pg = svptrue_b32();
  for (int32_t slice = 0; slice < rows; ++slice) {
    svld1_hor_za32(0, slice, pg, in + slice * n * 2);
    svld1_hor_za32(1, slice, pg, in + slice * n * 2 + 16);
    if constexpr (UnrollTwo) {
      svld1_hor_za32(2, slice, pg, in + slice * n * 2 + 32);
      svld1_hor_za32(3, slice, pg, in + slice * n * 2 + 48);
    }
  }
}

template<int TileCount>
__attribute__((noinline)) void load_u_layout_points_to_za(
    const svbool_t pg, const float32_t *in, const int64_t howmany,
    const int64_t point) __arm_streaming __arm_out("za") {
  for (int32_t slice = 0; slice < 16; ++slice) {
    svld1_hor_za32(0, slice, pg, in + (point + slice) * howmany * 2);
    if constexpr (TileCount >= 2) {
      svld1_hor_za32(1, slice, pg, in + (point + 16 + slice) * howmany * 2);
    }
    if constexpr (TileCount >= 3) {
      svld1_hor_za32(2, slice, pg, in + (point + 32 + slice) * howmany * 2);
    }
    if constexpr (TileCount >= 4) {
      svld1_hor_za32(3, slice, pg, in + (point + 48 + slice) * howmany * 2);
    }
  }
}

PLFFT_ALWAYS_INLINE void
store_vertical_tile_vg4(const svbool_t pg, float32_t *base,
                        const int64_t pair_stride, const int64_t tile_stride,
                        const int64_t tile,
                        const svfloat32x4_t data) __arm_streaming {
  float32_t *tile_base = base + tile * tile_stride;
  svst2_f32(pg, tile_base, svcreate2(svget4(data, 0), svget4(data, 1)));
  svst2_f32(pg, tile_base + pair_stride,
            svcreate2(svget4(data, 2), svget4(data, 3)));
}

PLFFT_ALWAYS_INLINE void
store_vertical_tile_vg2(const svbool_t pg, float32_t *base,
                        const int64_t tile_stride, const int64_t tile,
                        const svfloat32x2_t data) __arm_streaming {
  svst2_f32(pg, base + tile * tile_stride,
            svcreate2(svget2(data, 0), svget2(data, 1)));
}

template<int TileCount>
__attribute__((noinline)) void store_za_columns_to_output(
    const svbool_t pg, float32_t *out, const int64_t pair_stride,
    const int64_t tile_stride) __arm_streaming __arm_in("za") {
  for (int32_t slice = 0; slice < 16; slice += 4) {
    float32_t *slice_out = out + (slice / 2) * pair_stride;
    store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 0,
                            svread_ver_za32_f32_vg4(0, slice));
    if constexpr (TileCount >= 2) {
      store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 1,
                              svread_ver_za32_f32_vg4(1, slice));
    }
    if constexpr (TileCount >= 3) {
      store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 2,
                              svread_ver_za32_f32_vg4(2, slice));
    }
    if constexpr (TileCount >= 4) {
      store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 3,
                              svread_ver_za32_f32_vg4(3, slice));
    }
  }
}

template<int TileCount>
__attribute__((noinline)) void store_za_tail_columns_to_output(
    float32_t *out, const int64_t pair_stride, const int64_t tile_stride,
    const int64_t columns) __arm_streaming __arm_in("za") {
  const svbool_t pg = svptrue_b32();
  const int64_t column_pairs = columns / 2;
  int32_t slice = 0;
  for (int64_t col = 0; col < column_pairs; ++col) {
    float32_t *slice_out = out + (slice / 2) * pair_stride;
    store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 0,
                            svread_ver_za32_f32_vg4(0, slice));
    if constexpr (TileCount >= 2) {
      store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 1,
                              svread_ver_za32_f32_vg4(1, slice));
    }
    if constexpr (TileCount >= 3) {
      store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 2,
                              svread_ver_za32_f32_vg4(2, slice));
    }
    if constexpr (TileCount >= 4) {
      store_vertical_tile_vg4(pg, slice_out, pair_stride, tile_stride, 3,
                              svread_ver_za32_f32_vg4(3, slice));
    }
    slice += 4;
  }

  if (columns & 1) {
    float32_t *slice_out = out + (slice / 2) * pair_stride;
    store_vertical_tile_vg2(pg, slice_out, tile_stride, 0,
                            svread_ver_za32_f32_vg2(0, slice));
    if constexpr (TileCount >= 2) {
      store_vertical_tile_vg2(pg, slice_out, tile_stride, 1,
                              svread_ver_za32_f32_vg2(1, slice));
    }
    if constexpr (TileCount >= 3) {
      store_vertical_tile_vg2(pg, slice_out, tile_stride, 2,
                              svread_ver_za32_f32_vg2(2, slice));
    }
    if constexpr (TileCount >= 4) {
      store_vertical_tile_vg2(pg, slice_out, tile_stride, 3,
                              svread_ver_za32_f32_vg2(3, slice));
    }
  }
}

} // namespace plfft
