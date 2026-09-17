/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstddef>
#include <optional>

#include "plfft_move_vector.hpp"
#include "plfft_static_hash_map.hpp"
#include "policy.hpp"
#include "problem.hpp"

namespace plfft {

struct problem_hash {
  std::size_t operator()(const problem &p) const;
};

class wisdom {
public:
  struct entry {
    std::size_t strategy_idx;
    plfft::policy policy;
  };

  std::optional<entry> lookup(const problem &p, policy requested_policy);
  void update(const problem &p, entry entry);

private:
  using entries = move_vector<entry>;

  static_hash_map<problem, entries, 64, problem_hash> table;
};

} // namespace plfft
