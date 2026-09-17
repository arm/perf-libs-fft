/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace sloejit {

// Register a JIT-generated function with GDB.
void add_entry(void *sym_addr, int sym_size, const char *sym_name);

} // namespace sloejit
