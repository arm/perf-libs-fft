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

static constexpr int MN = 2;

void z2_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
           fresh_atom_factory &faf) {
  auto locals = faf.get_many(2);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[num + i]));
    algo.push_back(expr_t(locals[1], X[i], '-', X[num + i]));
    algo.push_back(expr_t(S[i], locals[0]));
    algo.push_back(expr_t(S[num + i], locals[1]));
  }
}

void z2_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], X[num + i]));
  }
}

std::vector<std::complex<double>> z2_mult() {
  std::vector<std::complex<double>> C(MN);
  C[0] = 1.0; // real
  C[1] = 1.0; // real
  return C;
}

} // namespace plfft::wfta
