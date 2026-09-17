/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sloejit {

uint8_t *alloc_executable_memory(size_t size);

void dealloc_executable_memory(void *addr, size_t size);

} // namespace sloejit
