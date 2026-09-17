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

static constexpr int MN = 9;

void z7_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(17);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[1], X[num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[2], X[4 * num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[3], X[4 * num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[4], X[2 * num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[5], X[2 * num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[6], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[7], locals[6], '+', locals[4]));
    algo.push_back(expr_t(locals[8], locals[7], '+', X[i]));
    algo.push_back(expr_t(locals[9], locals[0], '-', locals[2]));
    algo.push_back(expr_t(locals[10], locals[2], '-', locals[4]));
    algo.push_back(expr_t(locals[11], locals[4], '-', locals[0]));
    algo.push_back(expr_t(locals[12], locals[1], '+', locals[3]));
    algo.push_back(expr_t(locals[13], locals[12], '+', locals[5]));
    algo.push_back(expr_t(locals[14], locals[1], '-', locals[3]));
    algo.push_back(expr_t(locals[15], locals[3], '-', locals[5]));
    algo.push_back(expr_t(locals[16], locals[5], '-', locals[1]));
    algo.push_back(expr_t(S[i], locals[8]));
    algo.push_back(expr_t(S[num + i], locals[7]));
    algo.push_back(expr_t(S[2 * num + i], locals[9]));
    algo.push_back(expr_t(S[3 * num + i], locals[10]));
    algo.push_back(expr_t(S[4 * num + i], locals[11]));
    algo.push_back(expr_t(S[5 * num + i], locals[13]));
    algo.push_back(expr_t(S[6 * num + i], locals[14]));
    algo.push_back(expr_t(S[7 * num + i], locals[15]));
    algo.push_back(expr_t(S[8 * num + i], locals[16]));
  }
}

void z7_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(19);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[num + i]));
    algo.push_back(expr_t(locals[1], locals[0], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[2], locals[1], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[3], locals[0], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[4], locals[3], '-', X[4 * num + i]));
    algo.push_back(expr_t(locals[5], locals[0], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[6], locals[5], '+', X[4 * num + i]));
    algo.push_back(expr_t(locals[7], X[5 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[8], locals[7], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[9], X[5 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[10], locals[9], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[11], X[5 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[12], locals[11], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[13], locals[2], '+', locals[8]));
    algo.push_back(expr_t(locals[14], locals[2], '-', locals[8]));
    algo.push_back(expr_t(locals[15], locals[4], '+', locals[10]));
    algo.push_back(expr_t(locals[16], locals[4], '-', locals[10]));
    algo.push_back(expr_t(locals[17], locals[6], '+', locals[12]));
    algo.push_back(expr_t(locals[18], locals[6], '-', locals[12]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[14]));
    algo.push_back(expr_t(S[2 * num + i], locals[16]));
    algo.push_back(expr_t(S[3 * num + i], locals[17]));
    algo.push_back(expr_t(S[4 * num + i], locals[18]));
    algo.push_back(expr_t(S[5 * num + i], locals[15]));
    algo.push_back(expr_t(S[6 * num + i], locals[13]));
  }
}

std::vector<std::complex<double>> z7_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;
  const double u = 2. * pi / 7.;

  C[0] = std::complex<double>(1.0, 0.0);
  C[1] =
      std::complex<double>((cos(u) + cos(2. * u) + cos(3. * u)) / 3. - 1., 0.);
  C[2] =
      std::complex<double>((2. * cos(u) - cos(2. * u) - cos(3. * u)) / 3., 0.);
  C[3] =
      std::complex<double>((cos(u) - 2. * cos(2. * u) + cos(3. * u)) / 3., 0.);
  C[4] =
      std::complex<double>((cos(u) + cos(2. * u) - 2. * cos(3. * u)) / 3., 0.);
  C[5] = std::complex<double>(0.0, (sin(u) + sin(2. * u) - sin(3. * u)) / 3.);
  C[6] =
      std::complex<double>(0.0, (2. * sin(u) - sin(2. * u) + sin(3. * u)) / 3.);
  C[7] =
      std::complex<double>(0.0, (sin(u) - 2. * sin(2. * u) - sin(3. * u)) / 3.);
  C[8] =
      std::complex<double>(0.0, (sin(u) + sin(2. * u) + 2. * sin(3. * u)) / 3.);
  return C;
}

} // namespace plfft::wfta
