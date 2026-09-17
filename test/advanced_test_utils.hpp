/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "arm_fft1d.hpp"
#include "test_utils.hpp"
#include <cinttypes>
#include <iostream>
#include <limits>
#include <vector>

struct IOLayout {
  const int64_t n, howmany, stride, dist;
};

struct TestCase {
  const int64_t n, howmany, istride, idist, ostride, odist;

  IOLayout ilayout() const {
    return {.n = n, .howmany = howmany, .stride = istride, .dist = idist};
  }

  IOLayout olayout() const {
    return {.n = n, .howmany = howmany, .stride = ostride, .dist = odist};
  }
};

void print_failure_header(const TestCase &c, double tol) {
  std::cerr << "Failed: n=" << c.n << ", howmany=" << c.howmany
            << ", istride=" << c.istride << ", idist=" << c.idist
            << ", ostride=" << c.ostride << ", odist=" << c.odist
            << ", tol=" << tol << '\n';
}

template<typename T>
void print_failure(int64_t k, int64_t i, std::complex<T> out,
                   std::complex<T> ref, double err) {
  std::cerr << "k=" << k;
  std::cerr << ", i=" << i;
  std::cerr << ", out=(" << +out.real() << "," << +out.imag() << ")";
  std::cerr << ", ref=(" << +ref.real() << "," << +ref.imag() << ")";
  std::cerr << ", err=" << err << '\n';
}

template<typename Tx, typename Ty>
int advanced_interface_test(const TestCase &c, Planner<Tx, Ty> *make_plan) {
  using real_t = remove_complex_t<Tx>;

  real_t nf = c.n;

  std::cout << "Test n = " << c.n << "\n";

  // Exact buffer sizes
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
  plan_f->execute(in.data(), out.data());

  auto plan_b = make_plan(c.n, c.howmany, c.ostride, c.odist, c.istride,
                          c.idist, PLFFT_BACKWARD, PLFFT_IO_MAY_ALIAS);
  plan_b->execute(out.data(), out2.data());

  // Check forwards-backwards round-trip
  bool failure = false;
  const real_t tol = default_tol<real_t>(c.n);
  for (int64_t b = 0; b < c.howmany; ++b) {
    for (int64_t i = 0; i < c.n; ++i) {
      real_t res = real(out2[b * c.idist + i * c.istride]);
      real_t ref = real(in[b * c.idist + i * c.istride]);

      /*
       * A forwards-backwards round trip in:
       * * floating-point arithmetic yields the input multiplied by n.
       * * in fixed-point arithmetic yields the input divided by n.
       *
       * (fixed-point transforms apply scaling to prevent overflow)
       *
       * Therefore, to compare res and ref, we need to scale one of them:
       *
       * * For floating-point: divide the result by n to match the reference.
       * * For fixed-point   : divide the reference by n to match the result.
       *
       * Note: although it may seem natural to instead multiply the result by
       *       n in the fixed-point case, doing so risks saturation/overflow.
       */
      const auto [success, err] = [&] {
        real_t scaled_res = std::is_integral_v<real_t> ? res : res / nf;
        real_t scaled_ref = std::is_integral_v<real_t> ? ref / nf : ref;
        return check(scaled_res, scaled_ref, tol);
      }();

      if (!success) {
        if (!failure) {
          print_failure_header(c, tol);
        }
        failure = true;
        print_failure(b, i, static_cast<Tx>(res), static_cast<Tx>(ref), err);
      }
    }
  }
  if (failure) {
    return EXIT_FAILURE;
  } else {
    std::cout << " ---- Success!\n";
    return EXIT_SUCCESS;
  }
}

template<typename Tx, typename Ty>
int advanced_interface_test(const TestCase &c) {
  return advanced_interface_test<Tx, Ty>(c,
                                         &plfft::make_batched_1d_plan<Tx, Ty>);
}

template<typename Tx, typename Ty>
int advanced_interface_test_sme(const TestCase &c) {
  return advanced_interface_test<Tx, Ty>(
      c, &plfft::make_batched_1d_plan_sme<Tx, Ty>);
}
