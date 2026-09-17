/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#ifndef NO_LIBCPP

#include <sstream>
#include <string>
#include <utility>

static inline std::string level_to_string_direct(const int64_t n1,
                                                 const int64_t n2,
                                                 const int64_t howmany) {
  std::ostringstream sstm;
  sstm << "(direct ";
  sstm << n1 << ' ' << n2 << ' ' << howmany << ')';
  return std::move(sstm).str();
}

#endif // NO_LIBCPP
