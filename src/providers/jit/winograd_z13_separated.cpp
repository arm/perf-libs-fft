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

void z13_in(std::list<expr_t> &algo, atom *S, const atom *X, int num,
            fresh_atom_factory &faf) {
  auto locals = faf.get_many(43);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[1 * num + i], '+', X[12 * num + i]));
    algo.push_back(expr_t(locals[1], X[2 * num + i], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[2], X[3 * num + i], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[3], X[4 * num + i], '+', X[9 * num + i]));
    algo.push_back(expr_t(locals[4], X[5 * num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[5], X[6 * num + i], '+', X[7 * num + i]));
    algo.push_back(expr_t(locals[6], X[1 * num + i], '-', X[12 * num + i]));
    algo.push_back(expr_t(locals[7], X[2 * num + i], '-', X[11 * num + i]));
    algo.push_back(expr_t(locals[8], X[3 * num + i], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[9], X[4 * num + i], '-', X[9 * num + i]));
    algo.push_back(expr_t(locals[10], X[5 * num + i], '-', X[8 * num + i]));
    algo.push_back(expr_t(locals[11], X[6 * num + i], '-', X[7 * num + i]));
    algo.push_back(expr_t(locals[12], locals[1], '+', locals[4]));
    algo.push_back(expr_t(locals[13], locals[12], '+', locals[5]));
    algo.push_back(expr_t(locals[14], locals[0], '+', locals[2]));
    algo.push_back(expr_t(locals[15], locals[14], '+', locals[3]));
    algo.push_back(expr_t(locals[16], locals[13], '+', locals[15]));
    algo.push_back(expr_t(locals[17], locals[7], '+', locals[10]));
    algo.push_back(expr_t(locals[18], locals[17], '+', locals[11]));
    algo.push_back(expr_t(locals[19], locals[6], '+', locals[8]));
    algo.push_back(expr_t(locals[20], locals[19], '-', locals[9]));
    algo.push_back(expr_t(locals[21], locals[1], '-', locals[5]));
    algo.push_back(expr_t(locals[22], locals[2], '-', locals[3]));
    algo.push_back(expr_t(locals[23], locals[0], '-', locals[3]));
    algo.push_back(expr_t(locals[24], locals[4], '-', locals[5]));
    algo.push_back(expr_t(locals[25], locals[21], '-', locals[22]));
    algo.push_back(expr_t(locals[26], locals[23], '-', locals[24]));
    algo.push_back(expr_t(locals[27], locals[21], '+', locals[22]));
    algo.push_back(expr_t(locals[28], locals[23], '+', locals[24]));
    algo.push_back(expr_t(locals[29], locals[7], '-', locals[11]));
    algo.push_back(expr_t(locals[30], locals[6], '-', locals[8]));
    algo.push_back(expr_t(locals[31], locals[7], '-', locals[10]));
    algo.push_back(expr_t(locals[32], locals[6], '+', locals[9]));
    algo.push_back(expr_t(locals[33], locals[10], '-', locals[11]));
    algo.push_back(expr_t(locals[34], locals[8], '+', locals[9]));
    algo.push_back(expr_t(locals[35], X[0 * num + i], '+', locals[16]));
    algo.push_back(expr_t(locals[36], locals[15], '-', locals[13]));
    algo.push_back(expr_t(locals[37], locals[18], '+', locals[20]));
    algo.push_back(expr_t(locals[38], locals[25], '+', locals[26]));
    algo.push_back(expr_t(locals[39], locals[27], '-', locals[28]));
    algo.push_back(expr_t(locals[40], locals[29], '+', locals[30]));
    algo.push_back(expr_t(locals[41], locals[31], '+', locals[32]));
    algo.push_back(expr_t(locals[42], locals[33], '-', locals[34]));
    algo.push_back(expr_t(S[0 * num + i], locals[35]));
    algo.push_back(expr_t(S[1 * num + i], locals[16]));
    algo.push_back(expr_t(S[2 * num + i], locals[36]));
    algo.push_back(expr_t(S[3 * num + i], locals[18]));
    algo.push_back(expr_t(S[4 * num + i], locals[20]));
    algo.push_back(expr_t(S[5 * num + i], locals[37]));
    algo.push_back(expr_t(S[6 * num + i], locals[25]));
    algo.push_back(expr_t(S[7 * num + i], locals[26]));
    algo.push_back(expr_t(S[8 * num + i], locals[38]));
    algo.push_back(expr_t(S[9 * num + i], locals[27]));
    algo.push_back(expr_t(S[10 * num + i], locals[28]));
    algo.push_back(expr_t(S[11 * num + i], locals[39]));
    algo.push_back(expr_t(S[12 * num + i], locals[29]));
    algo.push_back(expr_t(S[13 * num + i], locals[30]));
    algo.push_back(expr_t(S[14 * num + i], locals[40]));
    algo.push_back(expr_t(S[15 * num + i], locals[31]));
    algo.push_back(expr_t(S[16 * num + i], locals[32]));
    algo.push_back(expr_t(S[17 * num + i], locals[41]));
    algo.push_back(expr_t(S[18 * num + i], locals[33]));
    algo.push_back(expr_t(S[19 * num + i], locals[34]));
    algo.push_back(expr_t(S[20 * num + i], locals[42]));
  }
}

void z13_out(std::list<expr_t> &algo, atom *S, const atom *X, int num,
             fresh_atom_factory &faf) {
  auto locals = faf.get_many(51);
  for (int i = 0; i < num; i++) {
    algo.push_back(expr_t(locals[0], X[0 * num + i], '-', X[1 * num + i]));
    algo.push_back(expr_t(locals[1], X[7 * num + i], '+', X[6 * num + i]));
    algo.push_back(expr_t(locals[2], locals[1], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[3], X[7 * num + i], '+', X[8 * num + i]));
    algo.push_back(expr_t(locals[4], locals[3], '+', X[2 * num + i]));
    algo.push_back(expr_t(locals[5], X[8 * num + i], '-', X[6 * num + i]));
    algo.push_back(expr_t(locals[6], locals[5], '-', X[2 * num + i]));
    algo.push_back(expr_t(locals[7], locals[0], '+', X[9 * num + i]));
    algo.push_back(expr_t(locals[8], locals[7], '+', X[10 * num + i]));
    algo.push_back(expr_t(locals[9], locals[0], '-', X[10 * num + i]));
    algo.push_back(expr_t(locals[10], locals[9], '-', X[11 * num + i]));
    algo.push_back(expr_t(locals[11], locals[0], '-', X[9 * num + i]));
    algo.push_back(expr_t(locals[12], locals[11], '+', X[11 * num + i]));
    algo.push_back(expr_t(locals[13], X[12 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[14], X[13 * num + i], '-', X[14 * num + i]));
    algo.push_back(expr_t(locals[15], X[15 * num + i], '-', X[17 * num + i]));
    algo.push_back(expr_t(locals[16], X[16 * num + i], '-', X[17 * num + i]));
    algo.push_back(expr_t(locals[17], X[18 * num + i], '-', X[20 * num + i]));
    algo.push_back(expr_t(locals[18], X[19 * num + i], '+', X[20 * num + i]));
    algo.push_back(expr_t(locals[19], X[3 * num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[20], X[4 * num + i], '-', X[5 * num + i]));
    algo.push_back(expr_t(locals[21], locals[2], '+', locals[8]));
    algo.push_back(expr_t(locals[22], locals[4], '+', locals[10]));
    algo.push_back(expr_t(locals[23], locals[10], '-', locals[4]));
    algo.push_back(expr_t(locals[24], locals[6], '+', locals[12]));
    algo.push_back(expr_t(locals[25], locals[8], '-', locals[2]));
    algo.push_back(expr_t(locals[26], locals[12], '-', locals[6]));
    algo.push_back(expr_t(locals[27], locals[20], '-', locals[13]));
    algo.push_back(expr_t(locals[28], locals[27], '+', locals[15]));
    algo.push_back(expr_t(locals[29], locals[18], '-', locals[19]));
    algo.push_back(expr_t(locals[30], locals[29], '-', locals[16]));
    algo.push_back(expr_t(locals[31], locals[13], '+', locals[17]));
    algo.push_back(expr_t(locals[32], locals[31], '+', locals[20]));
    algo.push_back(expr_t(locals[33], locals[15], '+', locals[17]));
    algo.push_back(expr_t(locals[34], locals[33], '-', locals[20]));
    algo.push_back(expr_t(locals[35], locals[14], '-', locals[18]));
    algo.push_back(expr_t(locals[36], locals[35], '-', locals[19]));
    algo.push_back(expr_t(locals[37], locals[19], '+', locals[14]));
    algo.push_back(expr_t(locals[38], locals[37], '-', locals[16]));
    algo.push_back(expr_t(locals[39], locals[21], '-', locals[28]));
    algo.push_back(expr_t(locals[40], locals[22], '+', locals[30]));
    algo.push_back(expr_t(locals[41], locals[23], '-', locals[32]));
    algo.push_back(expr_t(locals[42], locals[24], '-', locals[34]));
    algo.push_back(expr_t(locals[43], locals[25], '+', locals[36]));
    algo.push_back(expr_t(locals[44], locals[26], '-', locals[38]));
    algo.push_back(expr_t(locals[45], locals[26], '+', locals[38]));
    algo.push_back(expr_t(locals[46], locals[25], '-', locals[36]));
    algo.push_back(expr_t(locals[47], locals[24], '+', locals[34]));
    algo.push_back(expr_t(locals[48], locals[23], '+', locals[32]));
    algo.push_back(expr_t(locals[49], locals[22], '-', locals[30]));
    algo.push_back(expr_t(locals[50], locals[21], '+', locals[28]));
    algo.push_back(expr_t(S[0 * num + i], X[0 * num + i]));
    algo.push_back(expr_t(S[12 * num + i], locals[39]));
    algo.push_back(expr_t(S[11 * num + i], locals[40]));
    algo.push_back(expr_t(S[10 * num + i], locals[41]));
    algo.push_back(expr_t(S[9 * num + i], locals[42]));
    algo.push_back(expr_t(S[8 * num + i], locals[43]));
    algo.push_back(expr_t(S[7 * num + i], locals[44]));
    algo.push_back(expr_t(S[6 * num + i], locals[45]));
    algo.push_back(expr_t(S[5 * num + i], locals[46]));
    algo.push_back(expr_t(S[4 * num + i], locals[47]));
    algo.push_back(expr_t(S[3 * num + i], locals[48]));
    algo.push_back(expr_t(S[2 * num + i], locals[49]));
    algo.push_back(expr_t(S[1 * num + i], locals[50]));
  }
}

std::vector<std::complex<double>> z13_mult() {
  std::vector<std::complex<double>> C(MN);
  C[0] = std::complex<double>(1.0, 0.0);
  C[1] = std::complex<double>(
      1.083333333333333333333333333333333333333333333333333333333, 0.0);
  C[2] = std::complex<double>(
      -0.300462606288665774426601772289207995520941381153770517725, 0.0);
  C[3] = std::complex<double>(
      0.0, 0.749279330626139026374046342384718131077966283864729989797);
  C[4] = std::complex<double>(
      0.0, 0.401002128321867216362724752526188645844036931381927348444);
  C[5] = std::complex<double>(
      0.0, 0.575140729474003121368385547455453388461001607623328669120);
  C[6] = std::complex<double>(
      0.524226639526582149007971708126694516650847077766743900067, 0.0);
  C[7] = std::complex<double>(
      0.516520780623489722840901288569017135705033622107910000210, 0.0);
  C[8] = std::complex<double>(
      0.007705858903092426167070419557677380945813455658833899857, 0.0);
  C[9] = std::complex<double>(
      0.427634046826569427930437623290030103403586855840544787138, 0.0);
  C[10] = std::complex<double>(
      0.151805972074384398632872461156873747379523413085854383866, 0.0);
  C[11] = std::complex<double>(
      0.579440018900963826563310084446903850783110268926399171004, 0.0);
  C[12] = std::complex<double>(
      0.0, 1.154395338132363442014722675758496929984539541613788190860);
  C[13] = std::complex<double>(
      0.0, 0.906552201712710168803490799774568327790473456149543054154);
  C[14] = std::complex<double>(
      0.0, 0.818570272945918087795090854563255436726710956728765535094);
  C[15] = std::complex<double>(
      0.0, 1.197136772604342809453845339978408362731244645240610710432);
  C[16] = std::complex<double>(
      0.0, 0.861311707417897455234213518783166869473416060355588054666);
  C[17] = std::complex<double>(
      0.0, 1.109154843837550728445445394767095471667482145819833191372);
  C[18] = std::complex<double>(
      0.0, 0.042741434471979367439122664219911432746705103626822519571);
  C[19] = std::complex<double>(
      0.0, -0.045240494294812713569277280991401458317057395793954999488);
  C[20] = std::complex<double>(
      0.0, 0.290584570891632640650354540203840034940771189091067656277);

  return C;
}

} // namespace plfft::wfta
