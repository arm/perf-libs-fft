/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cassert>

#ifdef NDEBUG
#define ASSERT(pred)                                                           \
  do {                                                                         \
    if (!(pred)) {                                                             \
      __builtin_unreachable();                                                 \
    }                                                                          \
  } while (0)
#else
#define ASSERT(pred) assert(pred)
#endif
