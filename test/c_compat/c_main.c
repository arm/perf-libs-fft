/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

// Test that we can link an AOT build without libstdc++
#include <stdint.h>

int c_test(const int64_t n, double eps);

int main() {
  return c_test(10, 0.0);
}
