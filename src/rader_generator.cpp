/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "rader_generator.hpp"

#include "plfft_pod_vector.hpp"

namespace plfft {

int64_t find_group_generator(int64_t n) {
  // just go through in order until we find one
  pod_vector<uint8_t> seen(n);
  for (int64_t cand = 2; cand < n; ++cand) {
    for (int64_t i = 0; i < n; ++i) {
      seen[i] = 0;
    }
    bool success = true;
    for (int64_t x = 1, i = 1; i < n; ++i, x = (x * cand) % n) {
      if (seen[x]) {
        success = false;
        break;
      }
      seen[x] = 1;
    }
    if (success) {
      return cand;
    }
  }
  return 0;
}

} // end namespace plfft
