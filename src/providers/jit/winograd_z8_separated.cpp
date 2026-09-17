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

static constexpr int MN = 8;

void z8_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(16);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[4 * num + i]));
    algo.push_back(expr_t(locals[1], X[i], '-', X[4 * num + i]));
    algo.push_back(expr_t(locals[2], X[2 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[3], X[2 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[4], X[num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[5], X[num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[6], X[3 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[7], X[3 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[8], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[9], locals[0], '-', locals[2]));
    algo.push_back(expr_t(locals[10], locals[4], '+', locals[6]));
    algo.push_back(expr_t(locals[11], locals[4], '-', locals[6]));
    algo.push_back(expr_t(locals[12], locals[8], '+', locals[10]));
    algo.push_back(expr_t(locals[13], locals[8], '-', locals[10]));
    algo.push_back(expr_t(locals[14], locals[5], '+', locals[7]));
    algo.push_back(expr_t(locals[15], locals[5], '-', locals[7]));
    algo.push_back(expr_t(S[i], locals[12]));
    algo.push_back(expr_t(S[num + i], locals[13]));
    algo.push_back(expr_t(S[2 * num + i], locals[9]));
    algo.push_back(expr_t(S[3 * num + i], locals[11]));
    algo.push_back(expr_t(S[4 * num + i], locals[1]));
    algo.push_back(expr_t(S[5 * num + i], locals[3]));
    algo.push_back(expr_t(S[6 * num + i], locals[14]));
    algo.push_back(expr_t(S[7 * num + i], locals[15]));
  }
}

void z8_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(10);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[2 * num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[1], X[2 * num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[2], X[4 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[3], X[4 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[4], X[5 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[5], X[5 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[6], locals[2], '+', locals[4]));
    algo.push_back(expr_t(locals[7], locals[2], '-', locals[4]));
    algo.push_back(expr_t(locals[8], locals[3], '+', locals[5]));
    algo.push_back(expr_t(locals[9], locals[3], '-', locals[5]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[7]));
    algo.push_back(expr_t(S[2 * num + i], locals[1]));
    algo.push_back(expr_t(S[3 * num + i], locals[8]));
    algo.push_back(expr_t(S[4 * num + i], X[num + i]));
    algo.push_back(expr_t(S[5 * num + i], locals[9]));
    algo.push_back(expr_t(S[6 * num + i], locals[0]));
    algo.push_back(expr_t(S[7 * num + i], locals[6]));
  }
}

std::vector<std::complex<double>> z8_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;
  const double u = 2. * pi / 8.;

  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(1.0, 0.0);
  C[2] = std::complex<double>(1.0, 0.0);
  C[3] = std::complex<double>(0.0, std::sin(2. * u));
  C[4] = std::complex<double>(1.0, 0.0);
  C[5] = std::complex<double>(0.0, std::sin(2. * u));
  C[6] = std::complex<double>(0.0, std::sin(u));
  C[7] = std::complex<double>(std::cos(u), 0.0);
  return C;
}

} // namespace plfft::wfta
