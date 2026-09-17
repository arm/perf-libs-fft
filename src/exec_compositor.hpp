/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "compositor.hpp"

namespace plfft {

template<typename Tx, typename Ty>
void execute(const composition<Tx, Ty> &comp, int64_t howmany, const Tx *X,
             Ty *Y, int64_t istride, int64_t ostride, int64_t idist,
             int64_t odist);

} // namespace plfft
