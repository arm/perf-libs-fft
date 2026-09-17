/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "plfft.h"
#include "plfft_kernels.hpp"
#include "providers/jit/print_algo.hpp"

namespace plfft {

template<typename Tx, typename Ty>
void generate_kernel(const wfta::options_t &options, int64_t n,
                     plfft_direction_t dir, wfta::twiddleness twiddle,
                     order_kind order, const wfta::known_layout_t &known_layout,
                     const io_mods_t &mods);
}
