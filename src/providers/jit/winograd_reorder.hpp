/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstdint>
#include <vector>

namespace plfft::wfta {

std::vector<int64_t> get_in_perm(const std::vector<int> &nx);
std::vector<int64_t> get_out_perm(const std::vector<int> &nx);

} // namespace plfft::wfta
