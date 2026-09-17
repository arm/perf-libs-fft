/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "r2r_test.hpp"

#include <cinttypes>

int main() {
  const plfft_r2r_kind_t kinds[] = {
      PLFFT_R2R_DCT_1, PLFFT_R2R_DCT_2, PLFFT_R2R_DCT_3, PLFFT_R2R_DCT_4,
      PLFFT_R2R_DST_1, PLFFT_R2R_DST_2, PLFFT_R2R_DST_3, PLFFT_R2R_DST_4,
      PLFFT_R2R_DHT,   PLFFT_R2R_R2HC,  PLFFT_R2R_HC2R,
  };

  int ret = EXIT_SUCCESS;
  for (int64_t n = 2; n < 129; n++) {
    for (auto kind : kinds) {
      ret = r2r_test<__fp16>(n, kind, 0.001);
      if (ret != EXIT_SUCCESS) {
        printf("Half-precision r2r %s test failure for n = %" PRId64 "\n",
               r2r_name(kind), n);
        return ret;
      }

      ret = r2r_test<float>(n, kind);
      if (ret != EXIT_SUCCESS) {
        printf("Single-precision r2r %s test failure for n = %" PRId64 "\n",
               r2r_name(kind), n);
        return ret;
      }

      ret = r2r_test<double>(n, kind);
      if (ret != EXIT_SUCCESS) {
        printf("Double-precision r2r %s test failure for n = %" PRId64 "\n",
               r2r_name(kind), n);
        return ret;
      }
    }
  }
}
