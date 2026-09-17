/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "algo.hpp"
#include "winograd.hpp"
#include <complex>
#include <vector>

namespace plfft::wfta {

static constexpr int MN = 6;

void z5_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(8);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[num + i], '+', X[4 * num + i]));
    algo.push_back(expr_t(locals[1], X[num + i], '-', X[4 * num + i]));
    algo.push_back(expr_t(locals[2], X[3 * num + i], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[3], X[3 * num + i], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[4], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[5], locals[0], '-', locals[2]));
    algo.push_back(expr_t(locals[6], locals[1], '+', locals[3]));
    algo.push_back(expr_t(locals[7], locals[4], '+', X[i]));
    algo.push_back(expr_t(S[i], locals[7]));
    algo.push_back(expr_t(S[num + i], locals[4]));
    algo.push_back(expr_t(S[2 * num + i], locals[5]));
    algo.push_back(expr_t(S[3 * num + i], locals[1]));
    algo.push_back(expr_t(S[4 * num + i], locals[6]));
    algo.push_back(expr_t(S[5 * num + i], locals[3]));
  }
}

void z5_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(9);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[num + i]));
    algo.push_back(expr_t(locals[1], locals[0], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[2], locals[0], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[3], X[3 * num + i], '-', X[4 * num + i]));
    algo.push_back(expr_t(locals[4], X[4 * num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[5], locals[1], '+', locals[3]));
    algo.push_back(expr_t(locals[6], locals[1], '-', locals[3]));
    algo.push_back(expr_t(locals[7], locals[2], '+', locals[4]));
    algo.push_back(expr_t(locals[8], locals[2], '-', locals[4]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[6]));
    algo.push_back(expr_t(S[2 * num + i], locals[8]));
    algo.push_back(expr_t(S[3 * num + i], locals[7]));
    algo.push_back(expr_t(S[4 * num + i], locals[5]));
  }
}

std::vector<std::complex<double>> z5_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;
  const double u = 2. * pi / 5.;
  C[0] = std::complex<double>(1.0, 0.0); // real
  C[1] = std::complex<double>((std::cos(u) + std::cos(2. * u)) / 2. - 1.,
                              0.); // real
  C[2] =
      std::complex<double>((std::cos(u) - std::cos(2. * u)) / 2., 0.); // real
  C[3] = std::complex<double>(0., std::sin(u) + std::sin(2. * u));     // imag
  C[4] = std::complex<double>(0., std::sin(2. * u));                   // imag
  C[5] = std::complex<double>(0., std::sin(u) - std::sin(2. * u));     // imag
  return C;
}

} // namespace plfft::wfta
