/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "expr.hpp"

#include <list>
#include <map>

namespace plfft::wfta {

/**
 * Perform type checking on the specified algorithm, filling in the .self_type
 * member for each expr.  Loads and stores are resolved to be in_type and
 * out_type specified respectively.
 */
std::map<atom, expr_type> type_check(std::list<expr_t> &algo,
                                     const io_ptr_t &iop, expr_type in_type,
                                     expr_type out_type);

} // namespace plfft::wfta
