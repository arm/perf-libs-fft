/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace plfft {

struct cpu_features {
  bool asimdhp = false;
  bool fcma = false;
  bool sve = false;
  bool sme = false;
  bool sme2 = false;
};

cpu_features get_cpu_features();

} // namespace plfft
