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

static constexpr int MN = 39;

void z19_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(83);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[1 * num + i], '+', X[18 * num + i]));
    algo.push_back(expr_t(locals[1], X[1 * num + i], '-', X[18 * num + i]));
    algo.push_back(expr_t(locals[2], X[2 * num + i], '+', X[17 * num + i]));
    algo.push_back(expr_t(locals[3], X[17 * num + i], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[4], X[4 * num + i], '+', X[15 * num + i]));
    algo.push_back(expr_t(locals[5], X[4 * num + i], '-', X[15 * num + i]));
    algo.push_back(expr_t(locals[6], X[8 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[7], X[11 * num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[8], X[16 * num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[9], X[16 * num + i], '-', X[3 * num + i]));
    algo.push_back(expr_t(locals[10], X[13 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[11], X[6 * num + i], '-', X[13 * num + i]));
    algo.push_back(expr_t(locals[12], X[7 * num + i], '+', X[12 * num + i]));
    algo.push_back(expr_t(locals[13], X[7 * num + i], '-', X[12 * num + i]));
    algo.push_back(expr_t(locals[14], X[14 * num + i], '+', X[5 * num + i]));
    algo.push_back(expr_t(locals[15], X[5 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[16], X[9 * num + i], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[17], X[9 * num + i], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[18], locals[0], '-', locals[12]));
    algo.push_back(expr_t(locals[19], locals[2], '-', locals[14]));
    algo.push_back(expr_t(locals[20], locals[4], '-', locals[16]));
    algo.push_back(expr_t(locals[21], locals[6], '-', locals[12]));
    algo.push_back(expr_t(locals[22], locals[8], '-', locals[14]));
    algo.push_back(expr_t(locals[23], locals[10], '-', locals[16]));
    algo.push_back(expr_t(locals[24], locals[0], '+', locals[6]));
    algo.push_back(expr_t(locals[25], locals[24], '+', locals[12]));
    algo.push_back(expr_t(locals[26], locals[2], '+', locals[8]));
    algo.push_back(expr_t(locals[27], locals[26], '+', locals[14]));
    algo.push_back(expr_t(locals[28], locals[4], '+', locals[10]));
    algo.push_back(expr_t(locals[29], locals[28], '+', locals[16]));
    algo.push_back(expr_t(locals[30], locals[18], '+', locals[20]));
    algo.push_back(expr_t(locals[31], locals[21], '+', locals[23]));
    algo.push_back(expr_t(locals[32], locals[25], '+', locals[27]));
    algo.push_back(expr_t(locals[33], locals[32], '+', locals[29]));
    algo.push_back(expr_t(locals[34], X[0 * num + i], '+', locals[33]));
    algo.push_back(expr_t(locals[35], locals[31], '+', locals[22]));
    algo.push_back(expr_t(locals[36], locals[30], '+', locals[19]));
    algo.push_back(expr_t(locals[37], locals[36], '-', locals[35]));
    algo.push_back(expr_t(locals[38], locals[31], '-', locals[22]));
    algo.push_back(expr_t(locals[39], locals[30], '-', locals[19]));
    algo.push_back(expr_t(locals[40], locals[39], '-', locals[38]));
    algo.push_back(expr_t(locals[41], locals[18], '-', locals[21]));
    algo.push_back(expr_t(locals[42], locals[20], '-', locals[23]));
    algo.push_back(expr_t(locals[43], locals[18], '-', locals[42]));
    algo.push_back(expr_t(locals[44], locals[43], '-', locals[22]));
    algo.push_back(expr_t(locals[45], locals[41], '+', locals[23]));
    algo.push_back(expr_t(locals[46], locals[45], '-', locals[19]));
    algo.push_back(expr_t(locals[47], locals[44], '-', locals[46]));
    algo.push_back(expr_t(locals[48], locals[25], '-', locals[29]));
    algo.push_back(expr_t(locals[49], locals[27], '-', locals[29]));
    algo.push_back(expr_t(locals[50], locals[48], '+', locals[49]));
    algo.push_back(expr_t(locals[51], locals[1], '-', locals[13]));
    algo.push_back(expr_t(locals[52], locals[3], '-', locals[15]));
    algo.push_back(expr_t(locals[53], locals[5], '-', locals[17]));
    algo.push_back(expr_t(locals[54], locals[7], '-', locals[13]));
    algo.push_back(expr_t(locals[55], locals[9], '-', locals[15]));
    algo.push_back(expr_t(locals[56], locals[11], '-', locals[17]));
    algo.push_back(expr_t(locals[57], locals[1], '+', locals[7]));
    algo.push_back(expr_t(locals[58], locals[57], '+', locals[13]));
    algo.push_back(expr_t(locals[59], locals[3], '+', locals[9]));
    algo.push_back(expr_t(locals[60], locals[59], '+', locals[15]));
    algo.push_back(expr_t(locals[61], locals[5], '+', locals[11]));
    algo.push_back(expr_t(locals[62], locals[61], '+', locals[17]));
    algo.push_back(expr_t(locals[63], locals[51], '+', locals[53]));
    algo.push_back(expr_t(locals[64], locals[54], '+', locals[56]));
    algo.push_back(expr_t(locals[65], locals[58], '+', locals[60]));
    algo.push_back(expr_t(locals[66], locals[65], '+', locals[62]));
    algo.push_back(expr_t(locals[67], locals[64], '+', locals[55]));
    algo.push_back(expr_t(locals[68], locals[63], '+', locals[52]));
    algo.push_back(expr_t(locals[69], locals[68], '-', locals[67]));
    algo.push_back(expr_t(locals[70], locals[64], '-', locals[55]));
    algo.push_back(expr_t(locals[71], locals[63], '-', locals[52]));
    algo.push_back(expr_t(locals[72], locals[71], '-', locals[70]));
    algo.push_back(expr_t(locals[73], locals[51], '-', locals[54]));
    algo.push_back(expr_t(locals[74], locals[53], '-', locals[56]));
    algo.push_back(expr_t(locals[75], locals[51], '-', locals[74]));
    algo.push_back(expr_t(locals[76], locals[75], '-', locals[55]));
    algo.push_back(expr_t(locals[77], locals[73], '+', locals[56]));
    algo.push_back(expr_t(locals[78], locals[77], '-', locals[52]));
    algo.push_back(expr_t(locals[79], locals[76], '-', locals[78]));
    algo.push_back(expr_t(locals[80], locals[58], '-', locals[62]));
    algo.push_back(expr_t(locals[81], locals[60], '-', locals[62]));
    algo.push_back(expr_t(locals[82], locals[80], '+', locals[81]));
    algo.push_back(expr_t(S[0 * num + i], locals[34]));
    algo.push_back(expr_t(S[1 * num + i], locals[33]));
    algo.push_back(expr_t(S[2 * num + i], locals[35]));
    algo.push_back(expr_t(S[3 * num + i], locals[36]));
    algo.push_back(expr_t(S[4 * num + i], locals[37]));
    algo.push_back(expr_t(S[5 * num + i], locals[38]));
    algo.push_back(expr_t(S[6 * num + i], locals[39]));
    algo.push_back(expr_t(S[7 * num + i], locals[40]));
    algo.push_back(expr_t(S[8 * num + i], locals[21]));
    algo.push_back(expr_t(S[9 * num + i], locals[41]));
    algo.push_back(expr_t(S[10 * num + i], locals[18]));
    algo.push_back(expr_t(S[11 * num + i], locals[23]));
    algo.push_back(expr_t(S[12 * num + i], locals[42]));
    algo.push_back(expr_t(S[13 * num + i], locals[20]));
    algo.push_back(expr_t(S[14 * num + i], locals[44]));
    algo.push_back(expr_t(S[15 * num + i], locals[46]));
    algo.push_back(expr_t(S[16 * num + i], locals[47]));
    algo.push_back(expr_t(S[17 * num + i], locals[48]));
    algo.push_back(expr_t(S[18 * num + i], locals[49]));
    algo.push_back(expr_t(S[19 * num + i], locals[50]));
    algo.push_back(expr_t(S[20 * num + i], locals[66]));
    algo.push_back(expr_t(S[21 * num + i], locals[67]));
    algo.push_back(expr_t(S[22 * num + i], locals[68]));
    algo.push_back(expr_t(S[23 * num + i], locals[69]));
    algo.push_back(expr_t(S[24 * num + i], locals[70]));
    algo.push_back(expr_t(S[25 * num + i], locals[71]));
    algo.push_back(expr_t(S[26 * num + i], locals[72]));
    algo.push_back(expr_t(S[27 * num + i], locals[54]));
    algo.push_back(expr_t(S[28 * num + i], locals[73]));
    algo.push_back(expr_t(S[29 * num + i], locals[51]));
    algo.push_back(expr_t(S[30 * num + i], locals[56]));
    algo.push_back(expr_t(S[31 * num + i], locals[74]));
    algo.push_back(expr_t(S[32 * num + i], locals[53]));
    algo.push_back(expr_t(S[33 * num + i], locals[76]));
    algo.push_back(expr_t(S[34 * num + i], locals[78]));
    algo.push_back(expr_t(S[35 * num + i], locals[79]));
    algo.push_back(expr_t(S[36 * num + i], locals[80]));
    algo.push_back(expr_t(S[37 * num + i], locals[81]));
    algo.push_back(expr_t(S[38 * num + i], locals[82]));
  }
}

void z19_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
             fresh_atom_factory &faf) {
  auto locals = faf.get_many(85);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[2 * num + i], '+', X[3 * num + i]));
    algo.push_back(expr_t(locals[1], X[5 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[2], X[15 * num + i], '+', X[16 * num + i]));
    algo.push_back(expr_t(locals[3], locals[0], '+', locals[1]));
    algo.push_back(expr_t(locals[4], X[2 * num + i], '+', X[4 * num + i]));
    algo.push_back(expr_t(locals[5], X[5 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[6], X[14 * num + i], '+', X[16 * num + i]));
    algo.push_back(expr_t(locals[7], X[8 * num + i], '-', locals[3]));
    algo.push_back(expr_t(locals[8], locals[4], '+', locals[5]));
    algo.push_back(expr_t(locals[9], X[11 * num + i], '-', locals[6]));
    algo.push_back(expr_t(locals[10], X[9 * num + i], '+', locals[2]));
    algo.push_back(expr_t(locals[11], locals[10], '+', locals[7]));
    algo.push_back(expr_t(locals[12], locals[8], '+', X[12 * num + i]));
    algo.push_back(expr_t(locals[13], locals[12], '+', locals[9]));
    algo.push_back(expr_t(locals[14], locals[4], '-', locals[5]));
    algo.push_back(expr_t(locals[15], locals[14], '+', locals[2]));
    algo.push_back(expr_t(locals[16], locals[7], '+', locals[8]));
    algo.push_back(expr_t(locals[17], locals[16], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[18], locals[17], '+', locals[6]));
    algo.push_back(expr_t(locals[19], locals[3], '+', X[13 * num + i]));
    algo.push_back(expr_t(locals[20], locals[19], '+', locals[9]));
    algo.push_back(expr_t(locals[21], locals[20], '+', locals[2]));
    algo.push_back(expr_t(locals[22], locals[0], '-', locals[1]));
    algo.push_back(expr_t(locals[23], locals[22], '+', locals[6]));
    algo.push_back(expr_t(locals[24], X[17 * num + i], '-', X[19 * num + i]));
    algo.push_back(expr_t(locals[25], X[18 * num + i], '-', X[19 * num + i]));
    algo.push_back(expr_t(locals[26], X[1 * num + i], '+', X[0 * num + i]));
    algo.push_back(expr_t(locals[27], locals[26], '+', locals[24]));
    algo.push_back(expr_t(locals[28], locals[26], '-', locals[24]));
    algo.push_back(expr_t(locals[29], locals[28], '-', locals[25]));
    algo.push_back(expr_t(locals[30], locals[26], '+', locals[25]));
    algo.push_back(expr_t(locals[31], X[21 * num + i], '+', X[22 * num + i]));
    algo.push_back(expr_t(locals[32], X[24 * num + i], '+', X[25 * num + i]));
    algo.push_back(expr_t(locals[33], X[34 * num + i], '+', X[35 * num + i]));
    algo.push_back(expr_t(locals[34], locals[31], '+', locals[32]));
    algo.push_back(expr_t(locals[35], X[21 * num + i], '+', X[23 * num + i]));
    algo.push_back(expr_t(locals[36], X[24 * num + i], '+', X[26 * num + i]));
    algo.push_back(expr_t(locals[37], X[33 * num + i], '+', X[35 * num + i]));
    algo.push_back(expr_t(locals[38], X[27 * num + i], '-', locals[34]));
    algo.push_back(expr_t(locals[39], locals[35], '+', locals[36]));
    algo.push_back(expr_t(locals[40], X[30 * num + i], '-', locals[37]));
    algo.push_back(expr_t(locals[41], X[28 * num + i], '+', locals[33]));
    algo.push_back(expr_t(locals[42], locals[41], '+', locals[38]));
    algo.push_back(expr_t(locals[43], locals[39], '+', X[31 * num + i]));
    algo.push_back(expr_t(locals[44], locals[43], '+', locals[40]));
    algo.push_back(expr_t(locals[45], locals[35], '-', locals[36]));
    algo.push_back(expr_t(locals[46], locals[45], '+', locals[33]));
    algo.push_back(expr_t(locals[47], locals[38], '+', locals[39]));
    algo.push_back(expr_t(locals[48], locals[47], '+', X[29 * num + i]));
    algo.push_back(expr_t(locals[49], locals[48], '+', locals[37]));
    algo.push_back(expr_t(locals[50], locals[34], '+', X[32 * num + i]));
    algo.push_back(expr_t(locals[51], locals[50], '+', locals[40]));
    algo.push_back(expr_t(locals[52], locals[51], '+', locals[33]));
    algo.push_back(expr_t(locals[53], locals[31], '-', locals[32]));
    algo.push_back(expr_t(locals[54], locals[53], '+', locals[37]));
    algo.push_back(expr_t(locals[55], X[36 * num + i], '-', X[38 * num + i]));
    algo.push_back(expr_t(locals[56], X[37 * num + i], '-', X[38 * num + i]));
    algo.push_back(expr_t(locals[57], X[20 * num + i], '+', locals[55]));
    algo.push_back(expr_t(locals[58], X[20 * num + i], '-', locals[55]));
    algo.push_back(expr_t(locals[59], locals[58], '-', locals[56]));
    algo.push_back(expr_t(locals[60], X[20 * num + i], '+', locals[56]));
    algo.push_back(expr_t(locals[61], locals[18], '-', locals[11]));
    algo.push_back(expr_t(locals[62], locals[61], '+', locals[27]));
    algo.push_back(expr_t(locals[63], locals[21], '-', locals[13]));
    algo.push_back(expr_t(locals[64], locals[63], '+', locals[29]));
    algo.push_back(expr_t(locals[65], locals[23], '-', locals[15]));
    algo.push_back(expr_t(locals[66], locals[65], '+', locals[30]));
    algo.push_back(expr_t(locals[67], locals[27], '-', locals[18]));
    algo.push_back(expr_t(locals[68], locals[29], '-', locals[21]));
    algo.push_back(expr_t(locals[69], locals[30], '-', locals[23]));
    algo.push_back(expr_t(locals[70], locals[11], '+', locals[27]));
    algo.push_back(expr_t(locals[71], locals[13], '+', locals[29]));
    algo.push_back(expr_t(locals[72], locals[15], '+', locals[30]));
    algo.push_back(expr_t(locals[73], locals[49], '-', locals[42]));
    algo.push_back(expr_t(locals[74], locals[73], '+', locals[57]));
    algo.push_back(expr_t(locals[75], locals[52], '-', locals[44]));
    algo.push_back(expr_t(locals[76], locals[75], '+', locals[59]));
    algo.push_back(expr_t(locals[77], locals[54], '-', locals[46]));
    algo.push_back(expr_t(locals[78], locals[77], '+', locals[60]));
    algo.push_back(expr_t(locals[79], locals[57], '-', locals[49]));
    algo.push_back(expr_t(locals[80], locals[59], '-', locals[52]));
    algo.push_back(expr_t(locals[81], locals[54], '-', locals[60]));
    algo.push_back(expr_t(locals[82], locals[42], '+', locals[57]));
    algo.push_back(expr_t(locals[83], locals[44], '+', locals[59]));
    algo.push_back(expr_t(locals[84], locals[46], '+', locals[60]));
    algo.push_back(expr_t(S[0 * num + i], X[0 * num + i]));
    algo.push_back(expr_t(S[1 * num + i], locals[62], '+', locals[74]));
    algo.push_back(expr_t(S[18 * num + i], locals[62], '-', locals[74]));
    algo.push_back(expr_t(S[2 * num + i], locals[72], '-', locals[84]));
    algo.push_back(expr_t(S[17 * num + i], locals[72], '+', locals[84]));
    algo.push_back(expr_t(S[3 * num + i], locals[69], '+', locals[81]));
    algo.push_back(expr_t(S[16 * num + i], locals[69], '-', locals[81]));
    algo.push_back(expr_t(S[4 * num + i], locals[71], '+', locals[83]));
    algo.push_back(expr_t(S[15 * num + i], locals[71], '-', locals[83]));
    algo.push_back(expr_t(S[5 * num + i], locals[66], '+', locals[78]));
    algo.push_back(expr_t(S[14 * num + i], locals[66], '-', locals[78]));
    algo.push_back(expr_t(S[6 * num + i], locals[68], '+', locals[80]));
    algo.push_back(expr_t(S[13 * num + i], locals[68], '-', locals[80]));
    algo.push_back(expr_t(S[7 * num + i], locals[67], '+', locals[79]));
    algo.push_back(expr_t(S[12 * num + i], locals[67], '-', locals[79]));
    algo.push_back(expr_t(S[8 * num + i], locals[70], '-', locals[82]));
    algo.push_back(expr_t(S[11 * num + i], locals[70], '+', locals[82]));
    algo.push_back(expr_t(S[9 * num + i], locals[64], '+', locals[76]));
    algo.push_back(expr_t(S[10 * num + i], locals[64], '-', locals[76]));
  }
}

std::vector<std::complex<double>> z19_mult() {
  std::vector<std::complex<double>> C(MN);
  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(-1.0555555555555556, 0.0);
  C[2] = std::complex<double>(.1775222851392708, 0.0);
  C[3] = std::complex<double>(-.1282007750219153, 0.0);
  C[4] = std::complex<double>(.0493215101173555, 0.0);
  C[5] = std::complex<double>(.5761101149100590, 0.0);
  C[6] = std::complex<double>(-.7499644965553628, 0.0);
  C[7] = std::complex<double>(-.1738543816453038, 0.0);
  C[8] = std::complex<double>(-2.1729997561977314, 0.0);
  C[9] = std::complex<double>(-1.7021211726914737, 0.0);
  C[10] = std::complex<double>(.4708785835062578, 0.0);
  C[11] = std::complex<double>(-2.0239400846888438, 0.0);
  C[12] = std::complex<double>(.1055164120166409, 0.0);
  C[13] = std::complex<double>(2.1294564967054848, 0.0);
  C[14] = std::complex<double>(-.7508754389737117, 0.0);
  C[15] = std::complex<double>(.1481281769515716, 0.0);
  C[16] = std::complex<double>(.8990036159252833, 0.0);
  C[17] = std::complex<double>(-.6214824677260278, 0.0);
  C[18] = std::complex<double>(-.7986935209871269, 0.0);
  C[19] = std::complex<double>(-.4733919962377183, 0.0);
  C[20] = std::complex<double>(0.0, -.2421610524189263);
  C[21] = std::complex<double>(0.0, -.0593686079675051);
  C[22] = std::complex<double>(0.0, .0125786882551762);
  C[23] = std::complex<double>(0.0, -.0467899197123289);
  C[24] = std::complex<double>(0.0, -.9375012191378236);
  C[25] = std::complex<double>(0.0, -.0501115370433529);
  C[26] = std::complex<double>(0.0, -.9876127561811766);
  C[27] = std::complex<double>(0.0, -1.1745786501205960);
  C[28] = std::complex<double>(0.0, 1.1114482296234993);
  C[29] = std::complex<double>(0.0, 2.2860268797440954);
  C[30] = std::complex<double>(0.0, .2642052325793094);
  C[31] = std::complex<double>(0.0, 2.1981792779352138);
  C[32] = std::complex<double>(0.0, 1.9339740453559042);
  C[33] = std::complex<double>(0.0, -.7482584709125489);
  C[34] = std::complex<double>(0.0, -.4782083564276887);
  C[35] = std::complex<double>(0.0, .2700501144848602);
  C[36] = std::complex<double>(0.0, -.3464235615954227);
  C[37] = std::complex<double>(0.0, -.8348542936068828);
  C[38] = std::complex<double>(0.0, -.3937592850674352);
  return C;
}

} // namespace plfft::wfta
