/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "irprinter.hpp"
#include "irvalue.hpp"
#include "sloejit/aarch64/aarch64.hpp"

#include <map>
#include <string>

namespace plfft::wfta {

template<bool IsSVE, bool IsSME>
void print_value(std::map<std::string, sloejit::function_ptr> &fns,
                 sloejit::stack_frame_info *frame_info,
                 sloejit::aarch64::instr_builder &ib,
                 std::vector<rodata_info> &data_ofs,
                 std::vector<uint8_t> &data_bytes, regmap_t &regmap,
                 ir_value v);

} // namespace plfft::wfta
