// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

#include "arm_fft1d.hpp"

namespace plfft {

void clean() {
  // There is no kernel cache to clean in AOT build - cleaning is a no-op
}
} // namespace plfft
