/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

namespace plfft::wfta {

/// Store details for the target we are currently generating for.
struct target_t {
  std::string name; ///< The canonical name of the target.
  bool has_fcma;    ///< Whether this target supports floating point complex
                    ///< instructions.
  bool has_sve;     ///< Whether this target supports SVE.
  bool has_sme;     ///< Whether this target is generated for SME streaming
                    ///< mode.
  std::optional<int> known_vector_length_bytes;
};

/// Lookup a target by name, or terminate on failure.
target_t lookup_target(std::string name);

} // namespace plfft::wfta
