/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "benchmarker.hpp"

#include "plfft_util.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace plfft {

benchmarker::benchmarker(double target_secs_total_, double margin_)
  : target_secs_total(target_secs_total_), margin(margin_),
    start_time(timer_start()) {}

bool benchmarker::timed_out() const {
  return elapsed_secs(start_time) >= target_secs_total;
}

bool benchmarker::prefers(const result &challenger,
                          const result &incumbent) const {
  const auto comparison = compare(challenger, incumbent);
  if (!comparison) {
    return challenger.mean < incumbent.mean;
  }

  return *comparison;
}

double benchmarker::elapsed_secs(time_point_t start_time) {
  return timer_end(start_time);
}

// Return the size and base offset, in elements, of the smallest contiguous
// buffer that can hold the layout described by `(n, howmany, stride, dist)`.
//
// `offset` is the position of the first element of the first transform within
// that buffer. It is nonzero when a negative `stride` or `dist` makes the
// layout extend before that logical first element.
std::pair<std::size_t, std::size_t>
benchmarker::describe_buffer(int64_t n, int64_t howmany, int64_t stride,
                             int64_t dist) {
  assert(n > 0);
  assert(howmany >= 0);

  howmany = std::max<int64_t>(howmany, 1);

  const auto last_transform = mul_assert_no_overflow(howmany - 1, dist);
  const auto last_element = mul_assert_no_overflow(n - 1, stride);
  const auto last = add_assert_no_overflow(last_transform, last_element);
  const auto [min_index, max_index] =
      std::minmax<int64_t>({0, last_transform, last_element, last});

  assert(min_index >= max_index - std::numeric_limits<int64_t>::max() + 1);
  assert(min_index != std::numeric_limits<int64_t>::min());
  const std::size_t length = max_index - min_index + 1;
  const std::size_t offset = -min_index;
  return {length, offset};
}

std::optional<bool> benchmarker::compare(const result &challenger,
                                         const result &incumbent) const {
  if (challenger.n < min_rounds || incumbent.n < min_rounds) {
    // inconclusive due to insufficient data
    return std::nullopt;
  }

  // Pass the incumbent first so a large v1_p favors the challenger.
  const auto test = statistics::welch_t_test(incumbent, challenger);
  if (!std::isfinite(test.v1_p)) {
    // inconclusive due to invalid test result (e.g. zero variance)
    return std::nullopt;
  }

  if (test.v1_p >= 1.0 - margin) {
    return true; // challenger is better
  }

  if (test.v1_p <= margin) {
    return false; // incumbent is better
  }

  // inconclusive due to insufficient statistical significance
  return std::nullopt;
}

} // namespace plfft
