/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "advanced_test_utils.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>

template<typename Tx, typename Ty>
int timed_c2c_test(
    const TestCase &c, int64_t iterations, std::ostream &timing_output,
    Planner<Tx, Ty> *make_plan = &plfft::make_batched_1d_plan<Tx, Ty>) {
  using real_t = remove_complex_t<Tx>;
  using clock = std::chrono::steady_clock;

  /* Fill in input as usual */
  const real_t nf = c.n;

  size_t in_size = (c.howmany - 1) * c.idist + (c.n - 1) * c.istride + 1;
  size_t out_size = (c.howmany - 1) * c.odist + (c.n - 1) * c.ostride + 1;

  std::vector<Tx> in(in_size);
  std::vector<Ty> out(out_size);
  std::vector<Tx> out2(in_size);

  for (int64_t b = 0; b < c.howmany; ++b) {
    for (int64_t i = 0; i < c.n; ++i) {
      in[b * c.idist + i * c.istride] =
          convert_to<real_t>{}(static_cast<double>(i + b + 1) / nf);
    }
  }

  auto plan_f = make_plan(c.n, c.howmany, c.istride, c.idist, c.ostride,
                          c.odist, PLFFT_FORWARD, PLFFT_IO_NO_ALIAS);
  auto plan_b = make_plan(c.n, c.howmany, c.ostride, c.odist, c.istride,
                          c.idist, PLFFT_BACKWARD, PLFFT_IO_MAY_ALIAS);

  if (!plan_f || !plan_b) {
    std::cerr << " Failed to create plan: n = " << c.n
              << ", howmany = " << c.howmany << "\n";
    return EXIT_FAILURE;
  }

  const auto forward_start = clock::now();
  for (int64_t i = 0; i < iterations; ++i) {
    plan_f->execute(in.data(), out.data());
  }
  const auto forward_end = clock::now();

  const auto backward_start = clock::now();
  for (int64_t i = 0; i < iterations; ++i) {
    plan_b->execute(out.data(), out2.data());
  }
  const auto backward_end = clock::now();

  const double forward_seconds =
      std::chrono::duration<double>(forward_end - forward_start).count();
  const double backward_seconds =
      std::chrono::duration<double>(backward_end - backward_start).count();
  const double forward_seconds_per_execute = forward_seconds / iterations;
  const double backward_seconds_per_execute = backward_seconds / iterations;

  /* Save and flush each row before running the next test case */
  timing_output << " " << c.n << "  " << c.howmany << " "
                << " " << forward_seconds << " "
                << " " << forward_seconds_per_execute << " "
                << " " << forward_seconds_per_execute / c.howmany << " "
                << " " << backward_seconds << " "
                << " " << backward_seconds_per_execute << " "
                << " " << backward_seconds_per_execute / c.howmany << " \n"
                << std::flush;
  if (!timing_output) {
    std::cerr << " Failed to write to c2c_q0_7_timings.txt \n";
    return EXIT_FAILURE;
  }

  bool failure = false;
  const real_t tol = default_tol<real_t>(c.n);
  for (int64_t b = 0; b < c.howmany; ++b) {
    for (int64_t i = 0; i < c.n; ++i) {
      real_t res = real(out2[b * c.idist + i * c.istride]);
      real_t ref = real(in[b * c.idist + i * c.istride]);

      real_t scaled_res = std::is_integral_v<real_t> ? res : res / nf;
      real_t scaled_ref = std::is_integral_v<real_t> ? ref / nf : ref;
      const auto [success, err] = check(scaled_res, scaled_ref, tol);

      if (!success) {
        if (!failure) {
          print_failure_header(c, tol);
        }
        failure = true;
        print_failure(b, i, static_cast<Tx>(res), static_cast<Tx>(ref), err);
      }
    }
  }
  if (failure)
    return EXIT_FAILURE;
  std::cout << " ---- Success!\n";
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  int64_t iterations = 100;
  if (argc > 2) {
    fprintf(stderr, " Run: %s [No. of execs] \n", argv[0]);
    return 1;
  }
  if (argc == 2)
    iterations = std::strtoll(argv[1], nullptr, 10);

  if (iterations < 1) {
    std::cerr << " No. of executions must be >= 1 \n";
    return EXIT_FAILURE;
  }

  const std::vector<TestCase> test_cases_c2c = {
      // clang-format off
      //    n  howmany  istride   idist  ostride   odist
      {     2,       1,       1,       2,       1,      2 },
      {     4,       1,       1,       4,       1,      4 },
      {     8,       1,       1,       8,       1,      8 },
      // clang-format on
  };

  std::ofstream outfile("c2c_q0_7_timings.txt", std::ios::trunc);
  if (!outfile) {
    std::cerr << " Failed to open c2c_q0_7_timings.txt for writing \n";
    return EXIT_FAILURE;
  }
  outfile << " No. of iterations = " << iterations
          << ", all time units in seconds. \n"
          << " n " << " howmany " << " forward_total "
          << " forward_avg_execute " << " forward_avg_fft "
          << " backward_total " << " backward_avg_execute "
          << " backward_avg_fft \n"
          << std::flush;

  for (const auto &tc : test_cases_c2c) {
    printf("Testing n = %" PRId64 ", howmany = %" PRId64 "\n", tc.n,
           tc.howmany);

    auto ret = timed_c2c_test<std::complex<int8_t>, std::complex<int8_t>>(
        tc, iterations, outfile);
    if (ret != EXIT_SUCCESS) {
      printf(" Q0.7 c2c failed for n = %" PRId64 "\n", tc.n);
      return ret;
    }
  }
  return EXIT_SUCCESS;
}
