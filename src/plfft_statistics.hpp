/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

namespace plfft::statistics {
struct normal_distribution {
  double mean = 0;
  double stddev = 1;
  double n = 0;
};

normal_distribution sample_normal_incremental(normal_distribution in, double x);

struct t_test_result {
  double t;
  double v;
  double v1_p; // probability if v=1
};

t_test_result welch_t_test(normal_distribution n1, normal_distribution n2);

} // end namespace plfft::statistics
