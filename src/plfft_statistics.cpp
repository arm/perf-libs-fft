/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft_statistics.hpp"
#include "plfft_util.hpp"
#include <cmath>

namespace plfft::statistics {

inline double mean_incremental(normal_distribution in, double x) {
  return in.mean * in.n / (in.n + 1) + x / (in.n + 1);
}

inline double sample_stddev_incremental(normal_distribution in, double m,
                                        double x) {
  double var = in.stddev * in.stddev;
  var = (m - in.mean) * (m - in.mean) +
        (in.n > 0 ? (var * (in.n - 1) + (x - m) * (x - m)) / in.n : (double)0);
  return std::sqrt(var);
}

normal_distribution sample_normal_incremental(normal_distribution in,
                                              double x) {
  double m = mean_incremental(in, x);
  double stddev = sample_stddev_incremental(in, m, x);
  return {.mean = m, .stddev = stddev, .n = in.n + 1};
}

t_test_result welch_t_test(normal_distribution n1, normal_distribution n2) {
  double n1_s2 = n1.stddev * n1.stddev, n2_s2 = n2.stddev * n2.stddev;
  double s_delta = n1_s2 / n1.n + n2_s2 / n2.n;
  double t = (n1.mean - n2.mean) / std::sqrt(s_delta);
  double v = s_delta * s_delta /
             ((n1_s2 * n1_s2 / (n1.n * (n1.n - 1))) +
              (n2_s2 * n2_s2 / (n2.n * (n2.n - 1))));
  double v1_p = 0.5 + (1.0 / consts::pi<double>)*std::atan(t);
  return {.t = t, .v = v, .v1_p = v1_p};
}

} // end namespace plfft::statistics
