/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft_util.hpp"

namespace plfft {

/// Find generator of a group [1, N] under multiplication by brute-force.
int64_t find_group_generator(int64_t n);

} // namespace plfft
