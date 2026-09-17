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

static constexpr int MN = 36;

void z17_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(67);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[1 * num + i], '+', X[16 * num + i]));
    algo.push_back(expr_t(locals[1], X[1 * num + i], '-', X[16 * num + i]));
    algo.push_back(expr_t(locals[2], X[3 * num + i], '+', X[14 * num + i]));
    algo.push_back(expr_t(locals[3], X[3 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[4], X[9 * num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[5], X[9 * num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[6], X[10 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[7], X[10 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[8], X[13 * num + i], '+', X[4 * num + i]));
    algo.push_back(expr_t(locals[9], X[13 * num + i], '-', X[4 * num + i]));
    algo.push_back(expr_t(locals[10], X[5 * num + i], '+', X[12 * num + i]));
    algo.push_back(expr_t(locals[11], X[5 * num + i], '-', X[12 * num + i]));
    algo.push_back(expr_t(locals[12], X[15 * num + i], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[13], X[15 * num + i], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[14], X[11 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[15], X[11 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[16], locals[0], '+', locals[8]));
    algo.push_back(expr_t(locals[17], locals[2], '+', locals[10]));
    algo.push_back(expr_t(locals[18], locals[4], '+', locals[12]));
    algo.push_back(expr_t(locals[19], locals[6], '+', locals[14]));
    algo.push_back(expr_t(locals[20], locals[16], '+', locals[18]));
    algo.push_back(expr_t(locals[21], locals[17], '+', locals[19]));
    algo.push_back(expr_t(locals[22], locals[0], '-', locals[8]));
    algo.push_back(expr_t(locals[23], locals[2], '-', locals[10]));
    algo.push_back(expr_t(locals[24], locals[4], '-', locals[12]));
    algo.push_back(expr_t(locals[25], locals[6], '-', locals[14]));
    algo.push_back(expr_t(locals[26], locals[16], '-', locals[18]));
    algo.push_back(expr_t(locals[27], locals[17], '-', locals[19]));
    algo.push_back(expr_t(locals[28], locals[20], '+', locals[21]));
    algo.push_back(expr_t(locals[29], locals[20], '-', locals[21]));
    algo.push_back(expr_t(locals[30], locals[23], '+', locals[25]));
    algo.push_back(expr_t(locals[31], locals[22], '+', locals[24]));
    algo.push_back(expr_t(locals[32], locals[31], '-', locals[30]));
    algo.push_back(expr_t(locals[33], locals[24], '-', locals[25]));
    algo.push_back(expr_t(locals[34], locals[22], '-', locals[23]));
    algo.push_back(expr_t(locals[35], locals[26], '+', locals[27]));
    algo.push_back(expr_t(locals[36], locals[1], '+', locals[5]));
    algo.push_back(expr_t(locals[37], locals[3], '+', locals[7]));
    algo.push_back(expr_t(locals[38], locals[1], '-', locals[5]));
    algo.push_back(expr_t(locals[39], locals[15], '-', locals[11]));
    algo.push_back(expr_t(locals[40], locals[9], '+', locals[13]));
    algo.push_back(expr_t(locals[41], locals[11], '+', locals[15]));
    algo.push_back(expr_t(locals[42], locals[9], '-', locals[13]));
    algo.push_back(expr_t(locals[43], locals[3], '-', locals[7]));
    algo.push_back(expr_t(locals[44], locals[36], '+', locals[37]));
    algo.push_back(expr_t(locals[45], locals[40], '+', locals[41]));
    algo.push_back(expr_t(locals[46], locals[44], '+', locals[45]));
    algo.push_back(expr_t(locals[47], locals[36], '-', locals[37]));
    algo.push_back(expr_t(locals[48], locals[40], '-', locals[41]));
    algo.push_back(expr_t(locals[49], locals[47], '+', locals[48]));
    algo.push_back(expr_t(locals[50], locals[38], '+', locals[39]));
    algo.push_back(expr_t(locals[51], locals[42], '+', locals[43]));
    algo.push_back(expr_t(locals[52], locals[50], '+', locals[51]));
    algo.push_back(expr_t(locals[53], locals[38], '-', locals[39]));
    algo.push_back(expr_t(locals[54], locals[42], '-', locals[43]));
    algo.push_back(expr_t(locals[55], locals[53], '+', locals[54]));
    algo.push_back(expr_t(locals[56], locals[1], '+', locals[9]));
    algo.push_back(expr_t(locals[57], locals[7], '+', locals[15]));
    algo.push_back(expr_t(locals[58], locals[51], '-', locals[45]));
    algo.push_back(expr_t(locals[59], locals[58], '+', locals[1]));
    algo.push_back(expr_t(locals[60], locals[59], '-', locals[57]));
    algo.push_back(expr_t(locals[61], locals[44], '-', locals[50]));
    algo.push_back(expr_t(locals[62], locals[61], '+', locals[7]));
    algo.push_back(expr_t(locals[63], locals[62], '+', locals[9]));
    algo.push_back(expr_t(locals[64], locals[63], '-', locals[15]));
    algo.push_back(expr_t(locals[65], locals[60], '+', locals[64]));
    algo.push_back(expr_t(locals[66], X[0 * num + i], '+', locals[28]));
    algo.push_back(expr_t(S[0 * num + i], locals[66]));
    algo.push_back(expr_t(S[1 * num + i], locals[22]));
    algo.push_back(expr_t(S[2 * num + i], locals[23]));
    algo.push_back(expr_t(S[3 * num + i], locals[24]));
    algo.push_back(expr_t(S[4 * num + i], locals[25]));
    algo.push_back(expr_t(S[5 * num + i], locals[26]));
    algo.push_back(expr_t(S[6 * num + i], locals[27]));
    algo.push_back(expr_t(S[7 * num + i], locals[28]));
    algo.push_back(expr_t(S[8 * num + i], locals[29]));
    algo.push_back(expr_t(S[9 * num + i], locals[30]));
    algo.push_back(expr_t(S[10 * num + i], locals[31]));
    algo.push_back(expr_t(S[11 * num + i], locals[32]));
    algo.push_back(expr_t(S[12 * num + i], locals[33]));
    algo.push_back(expr_t(S[13 * num + i], locals[34]));
    algo.push_back(expr_t(S[14 * num + i], locals[35]));
    algo.push_back(expr_t(S[15 * num + i], locals[44]));
    algo.push_back(expr_t(S[16 * num + i], locals[45]));
    algo.push_back(expr_t(S[17 * num + i], locals[46]));
    algo.push_back(expr_t(S[18 * num + i], locals[47]));
    algo.push_back(expr_t(S[19 * num + i], locals[48]));
    algo.push_back(expr_t(S[20 * num + i], locals[49]));
    algo.push_back(expr_t(S[21 * num + i], locals[50]));
    algo.push_back(expr_t(S[22 * num + i], locals[51]));
    algo.push_back(expr_t(S[23 * num + i], locals[52]));
    algo.push_back(expr_t(S[24 * num + i], locals[53]));
    algo.push_back(expr_t(S[25 * num + i], locals[54]));
    algo.push_back(expr_t(S[26 * num + i], locals[55]));
    algo.push_back(expr_t(S[27 * num + i], locals[56]));
    algo.push_back(expr_t(S[28 * num + i], locals[1]));
    algo.push_back(expr_t(S[29 * num + i], locals[9]));
    algo.push_back(expr_t(S[30 * num + i], locals[57]));
    algo.push_back(expr_t(S[31 * num + i], locals[7]));
    algo.push_back(expr_t(S[32 * num + i], locals[15]));
    algo.push_back(expr_t(S[33 * num + i], locals[60]));
    algo.push_back(expr_t(S[34 * num + i], locals[64]));
    algo.push_back(expr_t(S[35 * num + i], locals[65]));
  }
}

void z17_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
             fresh_atom_factory &faf) {
  auto locals = faf.get_many(74);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[9 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[1], X[10 * num + i], '-', X[11 * num + i]));
    algo.push_back(expr_t(locals[2], X[4 * num + i], '+', X[12 * num + i]));
    algo.push_back(expr_t(locals[3], X[12 * num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[4], X[2 * num + i], '+', X[13 * num + i]));
    algo.push_back(expr_t(locals[5], X[1 * num + i], '-', X[13 * num + i]));
    algo.push_back(expr_t(locals[6], X[14 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[7], X[14 * num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[8], X[7 * num + i], '+', X[0 * num + i]));
    algo.push_back(expr_t(locals[9], X[8 * num + i], '+', locals[8]));
    algo.push_back(expr_t(locals[10], locals[8], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[11], locals[0], '-', locals[2]));
    algo.push_back(expr_t(locals[12], locals[6], '+', locals[9]));
    algo.push_back(expr_t(locals[13], locals[1], '+', locals[3]));
    algo.push_back(expr_t(locals[14], locals[7], '+', locals[10]));
    algo.push_back(expr_t(locals[15], locals[0], '+', locals[4]));
    algo.push_back(expr_t(locals[16], locals[9], '-', locals[6]));
    algo.push_back(expr_t(locals[17], locals[1], '+', locals[5]));
    algo.push_back(expr_t(locals[18], locals[10], '-', locals[7]));
    algo.push_back(expr_t(locals[19], locals[11], '+', locals[12]));
    algo.push_back(expr_t(locals[20], locals[13], '+', locals[14]));
    algo.push_back(expr_t(locals[21], locals[15], '+', locals[16]));
    algo.push_back(expr_t(locals[22], locals[17], '+', locals[18]));
    algo.push_back(expr_t(locals[23], locals[12], '-', locals[11]));
    algo.push_back(expr_t(locals[24], locals[14], '-', locals[13]));
    algo.push_back(expr_t(locals[25], locals[16], '-', locals[15]));
    algo.push_back(expr_t(locals[26], locals[18], '-', locals[17]));
    algo.push_back(expr_t(locals[27], X[15 * num + i], '+', X[17 * num + i]));
    algo.push_back(expr_t(locals[28], X[16 * num + i], '+', X[17 * num + i]));
    algo.push_back(expr_t(locals[29], X[18 * num + i], '+', X[20 * num + i]));
    algo.push_back(expr_t(locals[30], X[19 * num + i], '+', X[20 * num + i]));
    algo.push_back(expr_t(locals[31], X[21 * num + i], '+', X[23 * num + i]));
    algo.push_back(expr_t(locals[32], X[22 * num + i], '+', X[23 * num + i]));
    algo.push_back(expr_t(locals[33], X[24 * num + i], '+', X[26 * num + i]));
    algo.push_back(expr_t(locals[34], X[25 * num + i], '+', X[26 * num + i]));
    algo.push_back(expr_t(locals[35], X[35 * num + i], '+', X[34 * num + i]));
    algo.push_back(expr_t(locals[36], X[27 * num + i], '+', locals[35]));
    algo.push_back(expr_t(locals[37], locals[36], '+', X[28 * num + i]));
    algo.push_back(expr_t(locals[38], locals[27], '+', locals[29]));
    algo.push_back(expr_t(locals[39], locals[27], '-', locals[29]));
    algo.push_back(expr_t(locals[40], locals[28], '+', locals[30]));
    algo.push_back(expr_t(locals[41], locals[28], '-', locals[30]));
    algo.push_back(expr_t(locals[42], locals[31], '+', locals[33]));
    algo.push_back(expr_t(locals[43], locals[33], '-', locals[31]));
    algo.push_back(expr_t(locals[44], locals[32], '+', locals[34]));
    algo.push_back(expr_t(locals[45], locals[34], '-', locals[32]));
    algo.push_back(expr_t(locals[46], X[33 * num + i], '-', X[34 * num + i]));
    algo.push_back(expr_t(locals[47], locals[36], '+', X[29 * num + i]));
    algo.push_back(expr_t(locals[48], locals[46], '+', locals[46]));
    algo.push_back(expr_t(locals[49], X[30 * num + i], '-', locals[48]));
    algo.push_back(expr_t(locals[50], locals[49], '+', X[31 * num + i]));
    algo.push_back(expr_t(locals[51], locals[49], '+', X[32 * num + i]));
    algo.push_back(expr_t(locals[52], locals[35], '+', locals[35]));
    algo.push_back(expr_t(locals[53], locals[52], '+', locals[52]));
    algo.push_back(expr_t(locals[54], locals[46], '+', locals[52]));
    algo.push_back(expr_t(locals[55], locals[40], '+', locals[44]));
    algo.push_back(expr_t(locals[56], locals[55], '+', locals[47]));
    algo.push_back(expr_t(locals[57], locals[39], '+', locals[45]));
    algo.push_back(expr_t(locals[58], locals[57], '+', locals[50]));
    algo.push_back(expr_t(locals[59], locals[38], '-', locals[42]));
    algo.push_back(expr_t(locals[60], locals[59], '+', locals[52]));
    algo.push_back(expr_t(locals[61], locals[45], '-', locals[39]));
    algo.push_back(expr_t(locals[62], locals[61], '-', locals[54]));
    algo.push_back(expr_t(locals[63], locals[38], '+', locals[42]));
    algo.push_back(expr_t(locals[64], locals[63], '+', locals[37]));
    algo.push_back(expr_t(locals[65], locals[64], '+', locals[46]));
    algo.push_back(expr_t(locals[66], locals[43], '-', locals[41]));
    algo.push_back(expr_t(locals[67], locals[66], '-', locals[51]));
    algo.push_back(expr_t(locals[68], locals[67], '+', locals[53]));
    algo.push_back(expr_t(locals[69], locals[44], '-', locals[40]));
    algo.push_back(expr_t(locals[70], locals[69], '+', locals[48]));
    algo.push_back(expr_t(locals[71], locals[70], '+', locals[52]));
    algo.push_back(expr_t(locals[72], locals[41], '+', locals[43]));
    algo.push_back(expr_t(locals[73], locals[72], '-', locals[46]));
    algo.push_back(expr_t(S[0 * num + i], X[0 * num + i]));
    algo.push_back(expr_t(S[1 * num + i], locals[19], '+', locals[56]));
    algo.push_back(expr_t(S[16 * num + i], locals[19], '-', locals[56]));
    algo.push_back(expr_t(S[2 * num + i], locals[21], '+', locals[60]));
    algo.push_back(expr_t(S[15 * num + i], locals[21], '-', locals[60]));
    algo.push_back(expr_t(S[3 * num + i], locals[26], '+', locals[73]));
    algo.push_back(expr_t(S[14 * num + i], locals[26], '-', locals[73]));
    algo.push_back(expr_t(S[4 * num + i], locals[23], '+', locals[65]));
    algo.push_back(expr_t(S[13 * num + i], locals[23], '-', locals[65]));
    algo.push_back(expr_t(S[5 * num + i], locals[22], '+', locals[62]));
    algo.push_back(expr_t(S[12 * num + i], locals[22], '-', locals[62]));
    algo.push_back(expr_t(S[6 * num + i], locals[20], '+', locals[58]));
    algo.push_back(expr_t(S[11 * num + i], locals[20], '-', locals[58]));
    algo.push_back(expr_t(S[7 * num + i], locals[24], '+', locals[68]));
    algo.push_back(expr_t(S[10 * num + i], locals[24], '-', locals[68]));
    algo.push_back(expr_t(S[8 * num + i], locals[25], '+', locals[71]));
    algo.push_back(expr_t(S[9 * num + i], locals[25], '-', locals[71]));
  }
}

std::vector<std::complex<double>> z17_mult() {
  std::vector<std::complex<double>> C(MN);
  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(-0.0426028491177360, 0.0);
  C[2] = std::complex<double>(0.2049796502326218, 0.0);
  C[3] = std::complex<double>(1.0451835201736758, 0.0);
  C[4] = std::complex<double>(1.7645848660222969, 0.0);
  C[5] = std::complex<double>(-0.7234079772860566, 0.0);
  C[6] = std::complex<double>(-0.0890555916206064, 0.0);
  C[7] = std::complex<double>(-1.062500000000000, 0.0);
  C[8] = std::complex<double>(0.2576941016011038, 0.0);
  C[9] = std::complex<double>(0.7798026078948376, 0.0);
  C[10] = std::complex<double>(0.5438931846457058, 0.0);
  C[11] = std::complex<double>(0.4201019349705270, 0.0);
  C[12] = std::complex<double>(1.2810929434228074, 0.0);
  C[13] = std::complex<double>(0.4408890734817534, 0.0);
  C[14] = std::complex<double>(0.3171761928327251, 0.0);
  C[15] = std::complex<double>(0.0, -0.9013831864801668);
  C[16] = std::complex<double>(0.0, -0.4324875636007231);
  C[17] = std::complex<double>(0.0, 0.6669353750404450);
  C[18] = std::complex<double>(0.0, -0.6038900431251697);
  C[19] = std::complex<double>(0.0, -0.3692487319858255);
  C[20] = std::complex<double>(0.0, 0.4865693875554976);
  C[21] = std::complex<double>(0.0, 0.2381371213676061);
  C[22] = std::complex<double>(0.0, -1.5573820617422459);
  C[23] = std::complex<double>(0.0, 0.6596224701873199);
  C[24] = std::complex<double>(0.0, -0.1431696156986624);
  C[25] = std::complex<double>(0.0, 0.2390346995986077);
  C[26] = std::complex<double>(0.0, -0.0479325419499726);
  C[27] = std::complex<double>(0.0, -2.3188014856550064);
  C[28] = std::complex<double>(0.0, 0.7891456841920625);
  C[29] = std::complex<double>(0.0, 3.8484572871179504);
  C[30] = std::complex<double>(0.0, -1.3003804568801376);
  C[31] = std::complex<double>(0.0, 4.0814769046889033);
  C[32] = std::complex<double>(0.0, -1.4807159909286282);
  C[33] = std::complex<double>(0.0, -0.0133324703635514);
  C[34] = std::complex<double>(0.0, -0.3713977869055763);
  C[35] = std::complex<double>(0.0, 0.1923651286345638);
  return C;
}

} // namespace plfft::wfta
