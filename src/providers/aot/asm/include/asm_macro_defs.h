/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

// clang-format off
#ifdef __APPLE__
  #define PAGE(symbol) symbol@PAGE
  #define PAGEOFF(symbol) symbol@PAGEOFF

  #define RODATA_SECTION .section __DATA,__data

  #define FUNC_SECTION(fname) .section __TEXT,__text
  #define FUNC_TYPE(fname)
  #define FUNC_SIZE(fname)
  #define FUNC_NAME(fname) _##fname

#else
  #define HASH #

  #define PAGE(symbol) symbol
  #define PAGEOFF(symbol) HASH:lo12:symbol

  #ifndef PLFFT_ENABLE_ARM64EC
    #define FUNC_TYPE(fname) .type fname %function
    #define FUNC_SIZE(fname) .size fname, .-fname
    #define FUNC_NAME(fname) fname
    #define RODATA_SECTION() .section .rodata
    #define FUNC_SECTION(fname) .section .text.fname
  #else
    #define FUNC_TYPE(fname)
    #define FUNC_SIZE(fname)
    #define RODATA_SECTION() .section .rodata, "dr"
    #define FUNC_SECTION(fname) .section .text.fname, "xr"
  #endif
#endif
// clang-format on
