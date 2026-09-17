/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "kernel_cache.hpp"
#include "sloejit/util.hpp"

namespace plfft {

void kernel_cache::clean() {
  for (const auto &mem : allocated_mem) {
    sloejit::dealloc_executable_memory(std::get<0>(mem), std::get<1>(mem));
  }
  registry.clear();
  allocated_mem.clear();
}

} // end namespace plfft
