/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <arm_sme.h>

__arm_locally_streaming __arm_new("za") static void check_sme2() {
  svzero_za();
  const svfloat32x2_t rows = svread_hor_za32_f32_vg2(0, 0);
  (void)rows;
}

int main() {
  check_sme2();
  return 0;
}
