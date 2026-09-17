/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace plfft::config {
//@{
/** Get/set the kernel selection strategy (0 is the default).
 *  A strategy out of range for a particular problem size will
 *  cause a null plan to be returned.
 */
int get_kernel_strategy();
void set_kernel_strategy(int val);
//@}
} // namespace plfft::config
