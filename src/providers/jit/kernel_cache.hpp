/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "kernel_registry_entry.hpp"
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace plfft {

;

struct kernel_cache {
  static std::map<std::string, kernel_registry_entry<void>> registry;
  static std::vector<std::tuple<void *, size_t>> allocated_mem;

  static void clean(); // Only the declaration here
};

} // end namespace plfft
