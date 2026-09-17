/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include <arm_sme.h>

int main(void) {
  // Either SIGILL or false (== 0)
  return svcntsb() == 0;
}
