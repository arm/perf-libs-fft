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

static constexpr int MN = 11;

void z9_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(20);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[1], X[num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[2], X[7 * num + i], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[3], X[7 * num + i], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[4], X[3 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[5], X[3 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[6], X[4 * num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[7], X[4 * num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[8], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[9], locals[8], '+', locals[6]));
    algo.push_back(expr_t(locals[10], locals[9], '+', locals[4]));
    algo.push_back(expr_t(locals[11], locals[10], '+', X[i]));
    algo.push_back(expr_t(locals[12], locals[1], '+', locals[3]));
    algo.push_back(expr_t(locals[13], locals[12], '+', locals[7]));
    algo.push_back(expr_t(locals[14], locals[0], '-', locals[2]));
    algo.push_back(expr_t(locals[15], locals[2], '-', locals[6]));
    algo.push_back(expr_t(locals[16], locals[6], '-', locals[0]));
    algo.push_back(expr_t(locals[17], locals[1], '-', locals[3]));
    algo.push_back(expr_t(locals[18], locals[3], '-', locals[7]));
    algo.push_back(expr_t(locals[19], locals[7], '-', locals[1]));
    algo.push_back(expr_t(S[i], locals[11]));
    algo.push_back(expr_t(S[num + i], locals[9]));
    algo.push_back(expr_t(S[2 * num + i], locals[13]));
    algo.push_back(expr_t(S[3 * num + i], locals[4]));
    algo.push_back(expr_t(S[4 * num + i], locals[5]));
    algo.push_back(expr_t(S[5 * num + i], locals[14]));
    algo.push_back(expr_t(S[6 * num + i], locals[15]));
    algo.push_back(expr_t(S[7 * num + i], locals[16]));
    algo.push_back(expr_t(S[8 * num + i], locals[17]));
    algo.push_back(expr_t(S[9 * num + i], locals[18]));
    algo.push_back(expr_t(S[10 * num + i], locals[19]));
  }
}

void z9_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(25);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[num + i], '+', X[num + i]));
    algo.push_back(expr_t(locals[1], locals[0], '+', X[num + i]));
    algo.push_back(expr_t(locals[2], X[i], '+', locals[1]));
    algo.push_back(expr_t(locals[3], locals[2], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[4], locals[2], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[5], X[i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[6], locals[5], '+', locals[0]));
    algo.push_back(expr_t(locals[7], locals[6], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[8], locals[7], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[9], locals[6], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[10], locals[9], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[11], locals[6], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[12], locals[11], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[13], X[4 * num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[14], locals[13], '+', X[9 * num + i]));
    algo.push_back(expr_t(locals[15], X[4 * num + i], '-', X[9 * num + i]));
    algo.push_back(expr_t(locals[16], locals[15], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[17], X[4 * num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[18], locals[17], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[19], locals[8], '+', locals[14]));
    algo.push_back(expr_t(locals[20], locals[8], '-', locals[14]));
    algo.push_back(expr_t(locals[21], locals[10], '+', locals[16]));
    algo.push_back(expr_t(locals[22], locals[10], '-', locals[16]));
    algo.push_back(expr_t(locals[23], locals[12], '+', locals[18]));
    algo.push_back(expr_t(locals[24], locals[12], '-', locals[18]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[20]));
    algo.push_back(expr_t(S[2 * num + i], locals[21]));
    algo.push_back(expr_t(S[3 * num + i], locals[4]));
    algo.push_back(expr_t(S[4 * num + i], locals[24]));
    algo.push_back(expr_t(S[5 * num + i], locals[23]));
    algo.push_back(expr_t(S[6 * num + i], locals[3]));
    algo.push_back(expr_t(S[7 * num + i], locals[22]));
    algo.push_back(expr_t(S[8 * num + i], locals[19]));
  }
}

std::vector<std::complex<double>> z9_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;
  const double u = 2. * pi / 9.;
  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(-0.5, 0.);
  C[2] = std::complex<double>(0., std::sin(3. * u));
  C[3] = std::complex<double>(std::cos(3. * u) - 1., 0.);
  C[4] = std::complex<double>(0., std::sin(3. * u));
  C[5] = std::complex<double>(
      (2. * std::cos(u) - std::cos(2. * u) - std::cos(4. * u)) / 3., 0.0);
  C[6] = std::complex<double>(
      (std::cos(u) + std::cos(2. * u) - 2. * std::cos(4. * u)) / 3., 0.0);
  C[7] = std::complex<double>(
      (std::cos(u) - 2. * std::cos(2. * u) + std::cos(4. * u)) / 3., 0.0);
  C[8] = std::complex<double>(
      0.0, (2. * std::sin(u) + std::sin(2. * u) - std::sin(4. * u)) / 3.);
  C[9] = std::complex<double>(
      0.0, (std::sin(u) - std::sin(2. * u) - 2. * std::sin(4. * u)) / 3.);
  C[10] = std::complex<double>(
      0.0, (std::sin(u) + 2. * std::sin(2. * u) + std::sin(4. * u)) / 3.);
  return C;
}

} // namespace plfft::wfta
