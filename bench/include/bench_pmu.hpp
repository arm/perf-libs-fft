/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

enum class PMUMode { unsupported };

static inline std::string_view pmu_mode_values() {
  return "none (PMU support is not enabled in this build)";
}

static inline std::optional<PMUMode> parse_pmu_mode(std::string_view) {
  return std::nullopt;
}

static inline void pmu_start(PMUMode) {
  std::cerr << "error: PMU region markers are not supported by this build\n";
  std::exit(EXIT_FAILURE);
}

static inline uint64_t pmu_stop(PMUMode) {
  std::cerr << "error: PMU region markers are not supported by this build\n";
  std::exit(EXIT_FAILURE);
}
