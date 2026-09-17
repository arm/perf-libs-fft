/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace plfft {

// Whether the selected kernel provider can execute an FP16 transform on a CPU
// without native FP16 support.
bool kernel_provider_emulates_fp16();

} // namespace plfft
