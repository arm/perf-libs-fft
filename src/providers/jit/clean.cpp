// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

#include "arm_fft1d.hpp"
#include "providers/jit/kernel_cache.hpp"

namespace plfft {

void clean() {
  kernel_cache::clean();
}
} // namespace plfft
