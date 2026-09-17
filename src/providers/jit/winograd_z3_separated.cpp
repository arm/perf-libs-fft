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

static constexpr int MN = 3;

void z3_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(3);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[num + i], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[1], X[num + i], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[2], locals[0], '+', X[i]));
    algo.push_back(expr_t(S[i], locals[2]));
    algo.push_back(expr_t(S[num + i], locals[0]));
    algo.push_back(expr_t(S[2 * num + i], locals[1]));
  }
}

void z3_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(3);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[num + i]));
    algo.push_back(expr_t(locals[1], locals[0], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[2], locals[0], '-', X[2 * num + i]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[2]));
    algo.push_back(expr_t(S[2 * num + i], locals[1]));
  }
}

std::vector<std::complex<double>> z3_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;
  const double u = 2. * pi / 3.;
  C[0] = std::complex<double>(1.0, 0.);              // real
  C[1] = std::complex<double>(std::cos(u) - 1., 0.); // real
  C[2] = std::complex<double>(0.0, std::sin(u));     // imag
  return C;
}

} // namespace plfft::wfta
