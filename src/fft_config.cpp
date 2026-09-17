/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "fft_config.hpp"
#include "plfft_util.hpp"

namespace plfft::config {
static int kernel_strategy = 0;

int get_kernel_strategy() {
  return kernel_strategy;
}

void set_kernel_strategy(int val) {
  kernel_strategy = val;
}

} // namespace plfft::config
