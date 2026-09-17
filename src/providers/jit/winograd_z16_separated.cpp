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

static constexpr int MN = 18;

void z16_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(40);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[1], X[i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[2], X[4 * num + i], '+', X[12 * num + i]));
    algo.push_back(expr_t(locals[3], X[4 * num + i], '-', X[12 * num + i]));
    algo.push_back(expr_t(locals[4], X[2 * num + i], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[5], X[2 * num + i], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[6], X[6 * num + i], '+', X[14 * num + i]));
    algo.push_back(expr_t(locals[7], X[6 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[8], X[num + i], '+', X[9 * num + i]));
    algo.push_back(expr_t(locals[9], X[num + i], '-', X[9 * num + i]));
    algo.push_back(expr_t(locals[10], X[5 * num + i], '+', X[13 * num + i]));
    algo.push_back(expr_t(locals[11], X[5 * num + i], '-', X[13 * num + i]));
    algo.push_back(expr_t(locals[12], X[3 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[13], X[3 * num + i], '-', X[11 * num + i]));
    algo.push_back(expr_t(locals[14], X[7 * num + i], '+', X[15 * num + i]));
    algo.push_back(expr_t(locals[15], X[7 * num + i], '-', X[15 * num + i]));
    algo.push_back(expr_t(locals[16], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[17], locals[0], '-', locals[2]));
    algo.push_back(expr_t(locals[18], locals[4], '+', locals[6]));
    algo.push_back(expr_t(locals[19], locals[4], '-', locals[6]));
    algo.push_back(expr_t(locals[20], locals[8], '+', locals[10]));
    algo.push_back(expr_t(locals[21], locals[8], '-', locals[10]));
    algo.push_back(expr_t(locals[22], locals[12], '+', locals[14]));
    algo.push_back(expr_t(locals[23], locals[12], '-', locals[14]));
    algo.push_back(expr_t(locals[24], locals[16], '+', locals[18]));
    algo.push_back(expr_t(locals[25], locals[16], '-', locals[18]));
    algo.push_back(expr_t(locals[26], locals[20], '+', locals[22]));
    algo.push_back(expr_t(locals[27], locals[20], '-', locals[22]));
    algo.push_back(expr_t(locals[28], locals[24], '+', locals[26]));
    algo.push_back(expr_t(locals[29], locals[24], '-', locals[26]));
    algo.push_back(expr_t(locals[30], locals[21], '+', locals[23]));
    algo.push_back(expr_t(locals[31], locals[21], '-', locals[23]));
    algo.push_back(expr_t(locals[32], locals[5], '+', locals[7]));
    algo.push_back(expr_t(locals[33], locals[5], '-', locals[7]));
    algo.push_back(expr_t(locals[34], locals[9], '+', locals[15]));
    algo.push_back(expr_t(locals[35], locals[9], '-', locals[15]));
    algo.push_back(expr_t(locals[36], locals[11], '+', locals[13]));
    algo.push_back(expr_t(locals[37], locals[11], '-', locals[13]));
    algo.push_back(expr_t(locals[38], locals[34], '+', locals[36]));
    algo.push_back(expr_t(locals[39], locals[35], '+', locals[37]));
    algo.push_back(expr_t(S[i], locals[28]));
    algo.push_back(expr_t(S[num + i], locals[29]));
    algo.push_back(expr_t(S[2 * num + i], locals[25]));
    algo.push_back(expr_t(S[3 * num + i], locals[27]));
    algo.push_back(expr_t(S[4 * num + i], locals[17]));
    algo.push_back(expr_t(S[5 * num + i], locals[19]));
    algo.push_back(expr_t(S[6 * num + i], locals[30]));
    algo.push_back(expr_t(S[7 * num + i], locals[31]));
    algo.push_back(expr_t(S[8 * num + i], locals[1]));
    algo.push_back(expr_t(S[9 * num + i], locals[3]));
    algo.push_back(expr_t(S[10 * num + i], locals[32]));
    algo.push_back(expr_t(S[11 * num + i], locals[33]));
    algo.push_back(expr_t(S[12 * num + i], locals[38]));
    algo.push_back(expr_t(S[13 * num + i], locals[34]));
    algo.push_back(expr_t(S[14 * num + i], locals[36]));
    algo.push_back(expr_t(S[15 * num + i], locals[39]));
    algo.push_back(expr_t(S[16 * num + i], locals[35]));
    algo.push_back(expr_t(S[17 * num + i], locals[37]));
  }
}

void z16_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
             fresh_atom_factory &faf) {
  auto locals = faf.get_many(44);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[2 * num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[1], X[2 * num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[2], X[4 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[3], X[5 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[4], X[4 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[5], X[6 * num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[6], locals[2], '+', locals[3]));
    algo.push_back(expr_t(locals[7], locals[4], '+', locals[5]));
    algo.push_back(expr_t(locals[8], locals[4], '-', locals[5]));
    algo.push_back(expr_t(locals[9], locals[2], '-', locals[3]));
    algo.push_back(expr_t(locals[10], X[8 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[11], X[8 * num + i], '-', X[11 * num + i]));
    algo.push_back(expr_t(locals[12], X[9 * num + i], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[13], X[9 * num + i], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[14], X[12 * num + i], '+', X[13 * num + i]));
    algo.push_back(expr_t(locals[15], X[12 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[16], X[16 * num + i], '-', X[15 * num + i]));
    algo.push_back(expr_t(locals[17], X[17 * num + i], '-', X[15 * num + i]));
    algo.push_back(expr_t(locals[18], X[15 * num + i], '-', X[16 * num + i]));
    algo.push_back(expr_t(locals[19], X[15 * num + i], '-', X[17 * num + i]));
    algo.push_back(expr_t(locals[20], locals[10], '+', locals[16]));
    algo.push_back(expr_t(locals[21], locals[10], '-', locals[16]));
    algo.push_back(expr_t(locals[22], locals[10], '+', locals[18]));
    algo.push_back(expr_t(locals[23], locals[10], '-', locals[18]));
    algo.push_back(expr_t(locals[24], locals[11], '+', locals[13]));
    algo.push_back(expr_t(locals[25], locals[11], '-', locals[13]));
    algo.push_back(expr_t(locals[26], locals[11], '+', locals[19]));
    algo.push_back(expr_t(locals[27], locals[11], '-', locals[19]));
    algo.push_back(expr_t(locals[28], locals[14], '+', locals[12]));
    algo.push_back(expr_t(locals[29], locals[14], '-', locals[12]));
    algo.push_back(expr_t(locals[30], locals[14], '+', locals[12]));
    algo.push_back(expr_t(locals[31], locals[14], '-', locals[12]));
    algo.push_back(expr_t(locals[32], locals[15], '+', locals[17]));
    algo.push_back(expr_t(locals[33], locals[15], '-', locals[17]));
    algo.push_back(expr_t(locals[34], locals[15], '+', locals[13]));
    algo.push_back(expr_t(locals[35], locals[15], '-', locals[13]));
    algo.push_back(expr_t(locals[36], locals[20], '+', locals[28]));
    algo.push_back(expr_t(locals[37], locals[21], '+', locals[29]));
    algo.push_back(expr_t(locals[38], locals[22], '-', locals[31]));
    algo.push_back(expr_t(locals[39], locals[23], '-', locals[30]));
    algo.push_back(expr_t(locals[40], locals[24], '+', locals[32]));
    algo.push_back(expr_t(locals[41], locals[25], '+', locals[33]));
    algo.push_back(expr_t(locals[42], locals[26], '-', locals[35]));
    algo.push_back(expr_t(locals[43], locals[27], '-', locals[34]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[num + i], locals[39]));
    algo.push_back(expr_t(S[2 * num + i], locals[9]));
    algo.push_back(expr_t(S[3 * num + i], locals[42]));
    algo.push_back(expr_t(S[4 * num + i], locals[1]));
    algo.push_back(expr_t(S[5 * num + i], locals[43]));
    algo.push_back(expr_t(S[6 * num + i], locals[8]));
    algo.push_back(expr_t(S[7 * num + i], locals[38]));
    algo.push_back(expr_t(S[8 * num + i], X[num + i]));
    algo.push_back(expr_t(S[9 * num + i], locals[37]));
    algo.push_back(expr_t(S[10 * num + i], locals[7]));
    algo.push_back(expr_t(S[11 * num + i], locals[40]));
    algo.push_back(expr_t(S[12 * num + i], locals[0]));
    algo.push_back(expr_t(S[13 * num + i], locals[41]));
    algo.push_back(expr_t(S[14 * num + i], locals[6]));
    algo.push_back(expr_t(S[15 * num + i], locals[36]));
  }
}

std::vector<std::complex<double>> z16_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;
  const double u = 2. * pi / 16.;

  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(1.0, 0.0);
  C[2] = std::complex<double>(1.0, 0.0);
  C[3] = std::complex<double>(0.0, std::sin(4. * u));
  C[4] = std::complex<double>(1.0, 0.0);
  C[5] = std::complex<double>(0.0, std::sin(4. * u));
  C[6] = std::complex<double>(0.0, std::sin(2. * u));
  C[7] = std::complex<double>(std::cos(2. * u), 0.0);
  C[8] = std::complex<double>(1.0, 0.0);
  C[9] = std::complex<double>(0.0, std::sin(4. * u));
  C[10] = std::complex<double>(0.0, std::sin(2. * u));
  C[11] = std::complex<double>(std::cos(2. * u), 0.0);
  C[12] = std::complex<double>(0.0, std::sin(3. * u));
  C[13] = std::complex<double>(0.0, std::sin(u) - std::sin(3. * u));
  C[14] = std::complex<double>(0.0, std::sin(u) + std::sin(3. * u));
  C[15] = std::complex<double>(std::cos(3. * u), 0.0);
  C[16] = std::complex<double>(std::cos(u) + std::cos(3. * u), 0.0);
  C[17] = std::complex<double>(std::cos(3. * u) - std::cos(u), 0.0);
  return C;
}

} // namespace plfft::wfta
