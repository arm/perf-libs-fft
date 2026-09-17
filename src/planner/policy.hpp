/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstdint>

namespace plfft {

class policy {
public:
  constexpr explicit policy(uint32_t mask) : mask_{mask} {}

  static constexpr policy initial();
  static constexpr policy from_options(bool want_sme, double target_secs_total);

  constexpr bool contains(policy x) const {
    return (mask_ & x.mask_) == x.mask_;
  }

  constexpr bool is_subset_of(policy x) const {
    return (mask_ & x.mask_) == mask_;
  }

  constexpr bool operator==(const policy &x) const {
    return mask_ == x.mask_;
  }

  constexpr policy operator-(policy x) const {
    return policy{mask_ & ~x.mask_};
  }

  constexpr policy operator+(policy x) const {
    return policy{mask_ | x.mask_};
  }

private:
  uint32_t mask_;
};

constexpr policy ALLOW_CONVOLUTIONS{1u << 0}; // allow rader/bluestein
constexpr policy ALLOW_PRUNING{1u << 2}; // allow stopping after first candidate
constexpr policy ALLOW_SME{1u << 4};     // allow SME kernels
constexpr policy ALLOW_SME2{1u << 5};    // allow SME2 kernels

constexpr policy ALLOW_ALL =
    ALLOW_CONVOLUTIONS + ALLOW_SME + ALLOW_PRUNING + ALLOW_SME2;

constexpr policy policy::initial() {
  return ALLOW_ALL;
}

// Convert public planner options into the initial policy set.
constexpr policy policy::from_options(bool want_sme, double target_secs_total) {
  auto set = initial();
  if (!want_sme) {
    set = set - ALLOW_SME - ALLOW_SME2;
  }
  if (target_secs_total > 0.0) {
    set = set - ALLOW_PRUNING;
  }
  return set;
}

} // namespace plfft
