/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

// GCC 15 requires +sve2 with SME, but not all targets with SME support SVE
#if defined(__APPLE__)
#define PLFFT_TARGET_SME __attribute__((target("+sme")))
#else
#define PLFFT_TARGET_SME __attribute__((target("+sme+sve2")))
#endif

#define PLFFT_TARGET_SVE __attribute__((target("+sve")))

#define PLFFT_ALWAYS_INLINE inline __attribute__((always_inline))

#define ARM_LOCALLY_STREAMING_NEW_ZA __arm_locally_streaming __arm_new("za")
