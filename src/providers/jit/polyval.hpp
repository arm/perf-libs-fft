/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstring>

union __attribute__((packed)) polyval {
  char bytes[16];
  int8_t s8[16];
  __fp16 fp16[8];
  int16_t s16[8];
  float fp32[4];
  int32_t s32[4];
  double fp64[2];

  polyval() {
    static_assert(sizeof(bytes) == 16);
    static_assert(sizeof(s8) == 16);
    static_assert(sizeof(fp16) == 16);
    static_assert(sizeof(s16) == 16);
    static_assert(sizeof(fp32) == 16);
    static_assert(sizeof(s32) == 16);
    static_assert(sizeof(fp64) == 16);
    static_assert(sizeof(*this) == 16);
    memset(this, 0, sizeof(*this));
  }

  bool operator<(const polyval &other) const {
    static_assert(sizeof(*this) == 16);
    return memcmp(this, &other, sizeof(*this)) < 0;
  }

  bool operator==(const polyval &other) const {
    static_assert(sizeof(*this) == 16);
    return memcmp(this, &other, sizeof(*this)) == 0;
  }

  bool operator!=(const polyval &other) const {
    return !(*this == other);
  }
};
