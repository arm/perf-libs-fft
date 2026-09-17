/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "algo.hpp"
#include "winograd.hpp"
#include <vector>

namespace plfft::wfta {

void split_radix_z16(std::list<expr_t> &algo, atom *Y, const atom *X,
                     fresh_atom_factory &faf) {
  auto locals = faf.get_many(87);

  algo.push_back(expr_t(Y[0], X[0], '+', X[8]));
  algo.push_back(expr_t(Y[1], X[0], '-', X[8]));
  algo.push_back(expr_t(Y[2], X[4]));
  algo.push_back(expr_t(Y[3], X[12]));
  algo.push_back(expr_t(locals[0], Y[2], '+', Y[3]));
  algo.push_back(expr_t(locals[1], Y[2], '-', Y[3]));
  algo.push_back(expr_t(locals[2], locals[1], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[3], Y[0], '+', locals[0]));
  algo.push_back(expr_t(locals[4], Y[0], '-', locals[0]));
  algo.push_back(expr_t(locals[5], Y[1], '-', locals[2]));
  algo.push_back(expr_t(locals[6], Y[1], '+', locals[2]));
  algo.push_back(expr_t(Y[0], locals[3]));
  algo.push_back(expr_t(Y[1], locals[5]));
  algo.push_back(expr_t(Y[2], locals[4]));
  algo.push_back(expr_t(Y[3], locals[6]));
  algo.push_back(expr_t(Y[4], X[2], '+', X[10]));
  algo.push_back(expr_t(Y[5], X[2], '-', X[10]));
  algo.push_back(expr_t(Y[6], X[6], '+', X[14]));
  algo.push_back(expr_t(Y[7], X[6], '-', X[14]));
  algo.push_back(expr_t(locals[7], Y[4], '+', Y[6]));
  algo.push_back(expr_t(locals[8], Y[4], '-', Y[6]));
  algo.push_back(expr_t(locals[9], locals[8], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[10], Y[0], '+', locals[7]));
  algo.push_back(expr_t(locals[11], Y[0], '-', locals[7]));
  algo.push_back(expr_t(locals[12], Y[2], '-', locals[9]));
  algo.push_back(expr_t(locals[13], Y[2], '+', locals[9]));
  algo.push_back(expr_t(Y[0], locals[10]));
  algo.push_back(expr_t(Y[2], locals[12]));
  algo.push_back(expr_t(Y[4], locals[11]));
  algo.push_back(expr_t(Y[6], locals[13]));
  algo.push_back(expr_t(locals[14], Y[5], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[15], locals[14], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[16], Y[7], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[17], locals[16], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[18], locals[14], '+', locals[15]));
  algo.push_back(expr_t(locals[19], locals[16], '+', locals[17]));
  algo.push_back(expr_t(locals[20], locals[18], '+', locals[19]));
  algo.push_back(expr_t(locals[21], locals[19], '-', locals[18]));
  algo.push_back(
      expr_t(locals[22], locals[21], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[23], Y[1], '+', locals[20]));
  algo.push_back(expr_t(locals[24], Y[1], '-', locals[20]));
  algo.push_back(expr_t(locals[25], Y[3], '-', locals[22]));
  algo.push_back(expr_t(locals[26], Y[3], '+', locals[22]));
  algo.push_back(expr_t(Y[1], locals[23]));
  algo.push_back(expr_t(Y[3], locals[25]));
  algo.push_back(expr_t(Y[5], locals[24]));
  algo.push_back(expr_t(Y[7], locals[26]));
  algo.push_back(expr_t(Y[8], X[1], '+', X[9]));
  algo.push_back(expr_t(Y[9], X[1], '-', X[9]));
  algo.push_back(expr_t(Y[10], X[5]));
  algo.push_back(expr_t(Y[11], X[13]));
  algo.push_back(expr_t(locals[27], Y[10], '+', Y[11]));
  algo.push_back(expr_t(locals[28], Y[10], '-', Y[11]));
  algo.push_back(expr_t(locals[29], locals[28], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[30], Y[8], '+', locals[27]));
  algo.push_back(expr_t(locals[31], Y[8], '-', locals[27]));
  algo.push_back(expr_t(locals[32], Y[9], '-', locals[29]));
  algo.push_back(expr_t(locals[33], Y[9], '+', locals[29]));
  algo.push_back(expr_t(Y[8], locals[30]));
  algo.push_back(expr_t(Y[9], locals[32]));
  algo.push_back(expr_t(Y[10], locals[31]));
  algo.push_back(expr_t(Y[11], locals[33]));
  algo.push_back(expr_t(Y[12], X[3], '+', X[11]));
  algo.push_back(expr_t(Y[13], X[3], '-', X[11]));
  algo.push_back(expr_t(Y[14], X[7]));
  algo.push_back(expr_t(Y[15], X[15]));
  algo.push_back(expr_t(locals[34], Y[14], '+', Y[15]));
  algo.push_back(expr_t(locals[35], Y[14], '-', Y[15]));
  algo.push_back(expr_t(locals[36], locals[35], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[37], Y[12], '+', locals[34]));
  algo.push_back(expr_t(locals[38], Y[12], '-', locals[34]));
  algo.push_back(expr_t(locals[39], Y[13], '-', locals[36]));
  algo.push_back(expr_t(locals[40], Y[13], '+', locals[36]));
  algo.push_back(expr_t(Y[12], locals[37]));
  algo.push_back(expr_t(Y[13], locals[39]));
  algo.push_back(expr_t(Y[14], locals[38]));
  algo.push_back(expr_t(Y[15], locals[40]));
  algo.push_back(expr_t(locals[41], Y[8], '+', Y[12]));
  algo.push_back(expr_t(locals[42], Y[8], '-', Y[12]));
  algo.push_back(expr_t(locals[43], locals[42], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[44], Y[0], '+', locals[41]));
  algo.push_back(expr_t(locals[45], Y[0], '-', locals[41]));
  algo.push_back(expr_t(locals[46], Y[4], '-', locals[43]));
  algo.push_back(expr_t(locals[47], Y[4], '+', locals[43]));
  algo.push_back(expr_t(Y[0], locals[44]));
  algo.push_back(expr_t(Y[4], locals[46]));
  algo.push_back(expr_t(Y[8], locals[45]));
  algo.push_back(expr_t(Y[12], locals[47]));
  algo.push_back(expr_t(locals[48], Y[9], '*',
                        atom(RT_REAL_CONST, 0.923879532511286738483136105060)));
  algo.push_back(
      expr_t(locals[49], Y[9], '*',
             atom(RT_IMAG_CONST, -0.382683432365089781779232680492)));
  algo.push_back(expr_t(locals[50], Y[13], '*',
                        atom(RT_REAL_CONST, 0.382683432365089837290383911750)));
  algo.push_back(
      expr_t(locals[51], Y[13], '*',
             atom(RT_IMAG_CONST, -0.923879532511286738483136105060)));
  algo.push_back(expr_t(locals[52], locals[48], '+', locals[49]));
  algo.push_back(expr_t(locals[53], locals[50], '+', locals[51]));
  algo.push_back(expr_t(locals[54], locals[52], '+', locals[53]));
  algo.push_back(expr_t(locals[55], locals[53], '-', locals[52]));
  algo.push_back(
      expr_t(locals[56], locals[55], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[57], Y[1], '+', locals[54]));
  algo.push_back(expr_t(locals[58], Y[1], '-', locals[54]));
  algo.push_back(expr_t(locals[59], Y[5], '-', locals[56]));
  algo.push_back(expr_t(locals[60], Y[5], '+', locals[56]));
  algo.push_back(expr_t(Y[1], locals[57]));
  algo.push_back(expr_t(Y[5], locals[59]));
  algo.push_back(expr_t(Y[9], locals[58]));
  algo.push_back(expr_t(Y[13], locals[60]));
  algo.push_back(expr_t(locals[61], Y[10], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[62], locals[61], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[63], Y[14], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[64], locals[63], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[65], locals[61], '+', locals[62]));
  algo.push_back(expr_t(locals[66], locals[63], '+', locals[64]));
  algo.push_back(expr_t(locals[67], locals[65], '+', locals[66]));
  algo.push_back(expr_t(locals[68], locals[66], '-', locals[65]));
  algo.push_back(
      expr_t(locals[69], locals[68], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[70], Y[2], '+', locals[67]));
  algo.push_back(expr_t(locals[71], Y[2], '-', locals[67]));
  algo.push_back(expr_t(locals[72], Y[6], '-', locals[69]));
  algo.push_back(expr_t(locals[73], Y[6], '+', locals[69]));
  algo.push_back(expr_t(Y[2], locals[70]));
  algo.push_back(expr_t(Y[6], locals[72]));
  algo.push_back(expr_t(Y[10], locals[71]));
  algo.push_back(expr_t(Y[14], locals[73]));
  algo.push_back(expr_t(locals[74], Y[11], '*',
                        atom(RT_REAL_CONST, 0.382683432365089837290383911750)));
  algo.push_back(
      expr_t(locals[75], Y[11], '*',
             atom(RT_IMAG_CONST, -0.923879532511286738483136105060)));
  algo.push_back(
      expr_t(locals[76], Y[15], '*',
             atom(RT_REAL_CONST, -0.923879532511286849505438567576)));
  algo.push_back(expr_t(locals[77], Y[15], '*',
                        atom(RT_IMAG_CONST, 0.382683432365089670756930217976)));
  algo.push_back(expr_t(locals[78], locals[74], '+', locals[75]));
  algo.push_back(expr_t(locals[79], locals[76], '+', locals[77]));
  algo.push_back(expr_t(locals[80], locals[78], '+', locals[79]));
  algo.push_back(expr_t(locals[81], locals[79], '-', locals[78]));
  algo.push_back(
      expr_t(locals[82], locals[81], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[83], Y[3], '+', locals[80]));
  algo.push_back(expr_t(locals[84], Y[3], '-', locals[80]));
  algo.push_back(expr_t(locals[85], Y[7], '-', locals[82]));
  algo.push_back(expr_t(locals[86], Y[7], '+', locals[82]));
  algo.push_back(expr_t(Y[3], locals[83]));
  algo.push_back(expr_t(Y[7], locals[85]));
  algo.push_back(expr_t(Y[11], locals[84]));
  algo.push_back(expr_t(Y[15], locals[86]));
}

} // namespace plfft::wfta
