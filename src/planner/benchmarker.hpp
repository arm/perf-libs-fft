/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "plfft/fft_plan.hpp"
#include "plfft_pod_vector.hpp"
#include "plfft_statistics.hpp"
#include "plfft_timer.hpp"
#include "problem.hpp"

namespace plfft {

class benchmarker {
public:
  using result = statistics::normal_distribution;

  // The planning timer starts when the benchmarker is constructed.
  explicit benchmarker(double target_secs_total = 0.0, double margin = 0.05);

  bool timed_out() const;

  bool prefers(const result &challenger, const result &incumbent) const;

  template<typename Tx, typename Ty>
  std::optional<result>
  measure(const problem &p, fft_plan &plan,
          const std::optional<result> &incumbent_result = std::nullopt) const {
    // set up buffers
    const auto [_, prec, n, istride, ostride, howmany, idist, odist, dir] = p;
    const auto [x_sz, x_offset] = describe_buffer(n, howmany, istride, idist);
    const auto [y_sz, y_offset] = describe_buffer(n, howmany, ostride, odist);
    pod_vector<Tx> x_buffer(x_sz, Tx{});
    pod_vector<Ty> y_buffer(y_sz, Ty{});
    auto *x = x_buffer.data() + x_offset;
    auto *y = y_buffer.data() + y_offset;

    // calibrate and measure
    int niters = 1;
    int measured_rounds = 0;
    statistics::normal_distribution dist{};
    // measure until enough rounds have completed or search timeout is reached
    while (measured_rounds < max_rounds && !timed_out()) {
      // time one round of `niters` executions.
      const auto round_start = timer_start();
      for (int iter = 0; iter < niters; iter++) {
        plan.execute(x, y);
      }
      const auto round_secs = elapsed_secs(round_start);

      // calibrate `niters` to exceed clock-resolution noise
      if (measured_rounds == 0 && round_secs < min_round_secs) {
        niters *= 2;
        continue;
      }
      // calibration is complete. keep `niters` fixed while measuring

      // update the running statistics
      dist = statistics::sample_normal_incremental(dist, round_secs / niters);
      measured_rounds++;

      // stop early once the challenger is decisively better or worse
      if (incumbent_result) {
        const auto measured_result = dist;
        // compare() waits for min_rounds before making a decision
        if (compare(measured_result, *incumbent_result).has_value()) {
          return measured_result;
        }
      }
    }

    // no rounds completed before search timeout
    if (measured_rounds == 0) {
      return std::nullopt;
    }

    return dist;
  }

private:
  // minimum wall time for one measured round
  static constexpr double min_round_secs = 2.5e-4;
  // minimum and maximum measured rounds per candidate
  static constexpr int min_rounds = 3;
  static constexpr int max_rounds = 7;

  double target_secs_total;
  double margin;
  time_point_t start_time;

  static double elapsed_secs(time_point_t start_time);
  static std::pair<std::size_t, std::size_t>
  describe_buffer(int64_t n, int64_t howmany, int64_t stride, int64_t dist);
  std::optional<bool> compare(const result &challenger,
                              const result &incumbent) const;
};

} // namespace plfft
