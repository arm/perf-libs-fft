/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "vector_size.hpp"
#include "plfft_attrs.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PLFFT_ENABLE_SME
#include <arm_sme.h>
#endif

namespace {

#if PLFFT_ENABLE_SME
int PLFFT_TARGET_SME sme_vector_size_bytes() {
  return svcntsb();
}
#else
// Programs should always fail noisily if we call this
int sme_vector_size_bytes() {
  printf("Error: sme_vector_size_bytes() called, but this is not an "
         "SME-enabled build.\n");
  exit(1);
}
#endif

int PLFFT_TARGET_SVE sve_vector_size_bytes() {
  // use inline asm instead of ACLE so that non-SME build can support GCC 8
  uint64_t ret;
  __asm__ volatile("cntb %0" : "=r"(ret));
  return ret;
}
} // namespace

int plfft::vector_size_bytes(bool want_sme) {
  if (want_sme) {
    return sme_vector_size_bytes();
  }
  return sve_vector_size_bytes();
}
