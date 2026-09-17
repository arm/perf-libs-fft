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

static constexpr int MN = 4;

void z4_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(6);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[1], X[i], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[2], X[num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[3], X[num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[4], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[5], locals[0], '-', locals[2]));
    algo.push_back(expr_t(S[i], locals[4]));
    algo.push_back(expr_t(S[num + i], locals[5]));
    algo.push_back(expr_t(S[2 * num + i], locals[1]));
    algo.push_back(expr_t(S[3 * num + i], locals[3]));
  }
}

void z4_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(2);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[2 * num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[1], X[2 * num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[1]));
    algo.push_back(expr_t(S[2 * num + i], X[num + i]));
    algo.push_back(expr_t(S[3 * num + i], locals[0]));
  }
}

std::vector<std::complex<double>> z4_mult() {
  std::vector<std::complex<double>> C(MN);
  C[0] = std::complex<double>(1., 0.); // real
  C[1] = std::complex<double>(1., 0.); // real
  C[2] = std::complex<double>(1., 0.); // real
  C[3] = std::complex<double>(0., 1.); // imag
  return C;
}

} // namespace plfft::wfta
