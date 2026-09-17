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

static constexpr int MN = 21;

void z11_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(38);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[1], X[1 * num + i], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[2], X[2 * num + i], '+', X[9 * num + i]));
    algo.push_back(expr_t(locals[3], X[3 * num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[4], X[4 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[5], X[5 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[6], X[1 * num + i], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[7], X[2 * num + i], '-', X[9 * num + i]));
    algo.push_back(expr_t(locals[8], X[3 * num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[9], X[4 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[10], X[5 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[11], locals[1], '+', locals[2]));
    algo.push_back(expr_t(locals[12], locals[3], '+', locals[5]));
    algo.push_back(expr_t(locals[13], locals[4], '+', locals[11]));
    algo.push_back(expr_t(locals[14], locals[7], '-', locals[8]));
    algo.push_back(expr_t(locals[15], locals[6], '+', locals[10]));
    algo.push_back(expr_t(locals[16], locals[13], '+', locals[12]));
    algo.push_back(expr_t(locals[17], X[0 * num + i], '+', locals[16]));
    algo.push_back(expr_t(locals[18], locals[14], '-', locals[15]));
    algo.push_back(expr_t(locals[19], locals[18], '-', locals[9]));
    algo.push_back(expr_t(locals[20], locals[2], '-', locals[4]));
    algo.push_back(expr_t(locals[21], locals[1], '-', locals[4]));
    algo.push_back(expr_t(locals[22], locals[2], '-', locals[1]));
    algo.push_back(expr_t(locals[23], locals[5], '-', locals[4]));
    algo.push_back(expr_t(locals[24], locals[3], '-', locals[4]));
    algo.push_back(expr_t(locals[25], locals[5], '-', locals[3]));
    algo.push_back(expr_t(locals[26], locals[2], '-', locals[5]));
    algo.push_back(expr_t(locals[27], locals[1], '-', locals[3]));
    algo.push_back(expr_t(locals[28], locals[12], '-', locals[11]));
    algo.push_back(expr_t(locals[29], locals[7], '+', locals[9]));
    algo.push_back(expr_t(locals[30], locals[6], '-', locals[9]));
    algo.push_back(expr_t(locals[31], locals[6], '+', locals[7]));
    algo.push_back(expr_t(locals[32], locals[9], '-', locals[10]));
    algo.push_back(expr_t(locals[33], locals[8], '-', locals[9]));
    algo.push_back(expr_t(locals[34], locals[8], '-', locals[10]));
    algo.push_back(expr_t(locals[35], locals[7], '+', locals[10]));
    algo.push_back(expr_t(locals[36], locals[6], '-', locals[8]));
    algo.push_back(expr_t(locals[37], locals[14], '+', locals[15]));
    algo.push_back(expr_t(S[i], locals[17]));
    algo.push_back(expr_t(S[num + i], locals[16]));
    algo.push_back(expr_t(S[2 * num + i], locals[19]));
    algo.push_back(expr_t(S[3 * num + i], locals[20]));
    algo.push_back(expr_t(S[4 * num + i], locals[21]));
    algo.push_back(expr_t(S[5 * num + i], locals[22]));
    algo.push_back(expr_t(S[6 * num + i], locals[23]));
    algo.push_back(expr_t(S[7 * num + i], locals[24]));
    algo.push_back(expr_t(S[8 * num + i], locals[25]));
    algo.push_back(expr_t(S[9 * num + i], locals[26]));
    algo.push_back(expr_t(S[10 * num + i], locals[27]));
    algo.push_back(expr_t(S[11 * num + i], locals[28]));
    algo.push_back(expr_t(S[12 * num + i], locals[29]));
    algo.push_back(expr_t(S[13 * num + i], locals[30]));
    algo.push_back(expr_t(S[14 * num + i], locals[31]));
    algo.push_back(expr_t(S[15 * num + i], locals[32]));
    algo.push_back(expr_t(S[16 * num + i], locals[33]));
    algo.push_back(expr_t(S[17 * num + i], locals[34]));
    algo.push_back(expr_t(S[18 * num + i], locals[35]));
    algo.push_back(expr_t(S[19 * num + i], locals[36]));
    algo.push_back(expr_t(S[20 * num + i], locals[37]));
  }
}

void z11_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
             fresh_atom_factory &faf) {
  auto locals = faf.get_many(85);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[38], X[0 * num + i], '-', X[1 * num + i]));
    algo.push_back(expr_t(locals[39], X[3 * num + i], '+', X[4 * num + i]));
    algo.push_back(expr_t(locals[40], X[4 * num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[41], X[3 * num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[42], X[6 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[43], X[7 * num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[44], X[6 * num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[45], X[10 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[46], X[9 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[47], X[13 * num + i], '+', X[14 * num + i]));
    algo.push_back(expr_t(locals[48], X[12 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[49], X[16 * num + i], '+', X[17 * num + i]));
    algo.push_back(expr_t(locals[50], X[15 * num + i], '-', X[17 * num + i]));
    algo.push_back(expr_t(locals[51], X[19 * num + i], '+', X[20 * num + i]));
    algo.push_back(expr_t(locals[52], X[18 * num + i], '-', X[20 * num + i]));
    algo.push_back(expr_t(locals[53], locals[43], '+', locals[45]));
    algo.push_back(expr_t(locals[54], locals[53], '+', locals[38]));
    algo.push_back(expr_t(locals[55], locals[38], '-', locals[40]));
    algo.push_back(expr_t(locals[56], locals[55], '-', locals[45]));
    algo.push_back(expr_t(locals[57], locals[38], '+', locals[44]));
    algo.push_back(expr_t(locals[58], locals[57], '+', locals[46]));
    algo.push_back(expr_t(locals[59], locals[38], '-', locals[41]));
    algo.push_back(expr_t(locals[60], locals[59], '-', locals[46]));
    algo.push_back(expr_t(locals[61], locals[38], '+', locals[39]));
    algo.push_back(expr_t(locals[62], locals[61], '-', locals[42]));
    algo.push_back(expr_t(locals[63], X[2 * num + i], '+', locals[49]));
    algo.push_back(expr_t(locals[64], locals[63], '+', locals[51]));
    algo.push_back(expr_t(locals[65], locals[51], '-', locals[47]));
    algo.push_back(expr_t(locals[66], locals[65], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[67], X[2 * num + i], '+', locals[52]));
    algo.push_back(expr_t(locals[68], locals[67], '+', locals[50]));
    algo.push_back(expr_t(locals[69], locals[52], '-', locals[48]));
    algo.push_back(expr_t(locals[70], locals[69], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[71], locals[47], '+', locals[48]));
    algo.push_back(expr_t(locals[72], locals[71], '+', locals[49]));
    algo.push_back(expr_t(locals[73], locals[72], '+', locals[50]));
    algo.push_back(expr_t(locals[74], locals[73], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[75], locals[62], '+', locals[74]));
    algo.push_back(expr_t(locals[76], locals[54], '+', locals[64]));
    algo.push_back(expr_t(locals[77], locals[56], '+', locals[66]));
    algo.push_back(expr_t(locals[78], locals[58], '-', locals[68]));
    algo.push_back(expr_t(locals[79], locals[60], '+', locals[70]));
    algo.push_back(expr_t(locals[80], locals[60], '-', locals[70]));
    algo.push_back(expr_t(locals[81], locals[58], '+', locals[68]));
    algo.push_back(expr_t(locals[82], locals[56], '-', locals[66]));
    algo.push_back(expr_t(locals[83], locals[54], '-', locals[64]));
    algo.push_back(expr_t(locals[84], locals[62], '-', locals[74]));
    algo.push_back(expr_t(S[i], X[i]));
    algo.push_back(expr_t(S[10 * num + i], locals[75]));
    algo.push_back(expr_t(S[9 * num + i], locals[76]));
    algo.push_back(expr_t(S[8 * num + i], locals[77]));
    algo.push_back(expr_t(S[7 * num + i], locals[78]));
    algo.push_back(expr_t(S[6 * num + i], locals[79]));
    algo.push_back(expr_t(S[5 * num + i], locals[80]));
    algo.push_back(expr_t(S[4 * num + i], locals[81]));
    algo.push_back(expr_t(S[3 * num + i], locals[82]));
    algo.push_back(expr_t(S[2 * num + i], locals[83]));
    algo.push_back(expr_t(S[num + i], locals[84]));
  }
}

std::vector<std::complex<double>> z11_mult() {
  std::vector<std::complex<double>> C(MN);
  constexpr double pi = 3.141592653589793;

  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(1.1, 0.0);
  C[2] = std::complex<double>(
      0.0, 0.2 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                  std::sin(pi / 11.) + std::cos((pi / 22.)) +
                  std::cos(5. * pi / 22.)));
  C[3] = std::complex<double>(0.10 + std::sin(3. * pi / 22.), 0.0);
  C[4] = std::complex<double>(0.10 + std::cos(2. * pi / 11.), 0.0);
  C[5] = std::complex<double>(
      -0.2 + std::sin(5. * pi / 22.) - std::cos(10 * pi / 11), 0.0);
  C[6] = std::complex<double>(0.40 + std::sin(3. * pi / 22.) +
                                  std::cos(2. * pi / 11.) - std::sin(pi / 22.) -
                                  std::sin(5. * pi / 22.),
                              0.0);
  C[7] = std::complex<double>(std::sin(pi / 22.) - 0.1, 0.0);
  C[8] = std::complex<double>(
      0.2 + std::cos(2. * pi / 11.) - std::sin(5. * pi / 22.), 0.0);
  C[9] = std::complex<double>(std::cos(6. * pi / 11) + std::sin(5. * pi / 22.),
                              0.0);
  C[10] = std::complex<double>(
      std::sin(5. * pi / 22.) + std::sin(3. * pi / 22.), 0.0);
  C[11] = std::complex<double>(std::sin(5. * pi / 22.) - 0.10, 0.0);
  C[12] = std::complex<double>(
      0.0, std::cos(3. * pi / 22.) +
               0.2 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)));
  C[13] = std::complex<double>(
      0.0, std::sin(2. * pi / 11.) -
               0.2 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)));
  C[14] = std::complex<double>(
      0.0, std::cos(9. * pi / 22.) + std::cos(5. * pi / 22.) -
               0.4 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)));
  C[15] = std::complex<double>(
      0.0, 0.2 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                  std::sin(pi / 11.) + std::cos((pi / 22.)) +
                  std::cos(5. * pi / 22.)) -
               std::sin(pi / 11.));
  C[16] = std::complex<double>(
      0.0, std::cos(pi / 22.) -
               0.2 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)));
  C[17] = std::complex<double>(
      0.0, std::sin(2. * pi / 11.) -
               0.4 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)) +
               std::cos(5. * pi / 22.));
  C[18] = std::complex<double>(
      0.0, -std::cos(21. * pi / 22.) -
               0.4 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)) +
               std::cos(5. * pi / 22.));
  C[19] = std::complex<double>(
      0.0, std::cos(3. * pi / 22.) +
               0.4 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)) -
               std::cos(5. * pi / 22.));
  C[20] = std::complex<double>(
      0.0, std::cos(5. * pi / 22.) -
               0.2 * (-std::cos(3. * pi / 22.) + std::sin(2. * pi / 11.) +
                      std::sin(pi / 11.) + std::cos((pi / 22.)) +
                      std::cos(5. * pi / 22.)));

  return C;
}

} // namespace plfft::wfta
