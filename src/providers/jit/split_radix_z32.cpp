/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "algo.hpp"
#include "winograd.hpp"
#include <vector>

namespace plfft::wfta {

void split_radix_z32(std::list<expr_t> &algo, atom *Y, const atom *X,
                     fresh_atom_factory &faf) {
  auto locals = faf.get_many(259);

  algo.push_back(expr_t(Y[0], X[0], '+', X[16]));
  algo.push_back(expr_t(Y[1], X[0], '-', X[16]));
  algo.push_back(expr_t(Y[2], X[8]));
  algo.push_back(expr_t(Y[3], X[24]));
  algo.push_back(expr_t(locals[0], Y[2], '+', Y[3]));
  algo.push_back(expr_t(locals[1], Y[2], '-', Y[3]));
  algo.push_back(expr_t(locals[2], locals[1], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[147], Y[0], '+', locals[0]));
  algo.push_back(expr_t(locals[148], Y[0], '-', locals[0]));
  algo.push_back(expr_t(locals[149], Y[1], '-', locals[2]));
  algo.push_back(expr_t(locals[150], Y[1], '+', locals[2]));
  algo.push_back(expr_t(Y[0], locals[147]));
  algo.push_back(expr_t(Y[1], locals[149]));
  algo.push_back(expr_t(Y[2], locals[148]));
  algo.push_back(expr_t(Y[3], locals[150]));
  algo.push_back(expr_t(Y[4], X[4], '+', X[20]));
  algo.push_back(expr_t(Y[5], X[4], '-', X[20]));
  algo.push_back(expr_t(Y[6], X[12], '+', X[28]));
  algo.push_back(expr_t(Y[7], X[12], '-', X[28]));
  algo.push_back(expr_t(locals[3], Y[4], '+', Y[6]));
  algo.push_back(expr_t(locals[4], Y[4], '-', Y[6]));
  algo.push_back(expr_t(locals[5], locals[4], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[151], Y[0], '+', locals[3]));
  algo.push_back(expr_t(locals[152], Y[0], '-', locals[3]));
  algo.push_back(expr_t(locals[153], Y[2], '-', locals[5]));
  algo.push_back(expr_t(locals[154], Y[2], '+', locals[5]));
  algo.push_back(expr_t(Y[0], locals[151]));
  algo.push_back(expr_t(Y[2], locals[153]));
  algo.push_back(expr_t(Y[4], locals[152]));
  algo.push_back(expr_t(Y[6], locals[154]));
  algo.push_back(expr_t(locals[69], Y[5], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[70], locals[69], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[71], Y[7], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[72], locals[71], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[73], locals[69], '+', locals[70]));
  algo.push_back(expr_t(locals[74], locals[71], '+', locals[72]));
  algo.push_back(expr_t(locals[6], locals[73], '+', locals[74]));
  algo.push_back(expr_t(locals[7], locals[74], '-', locals[73]));
  algo.push_back(
      expr_t(locals[8], locals[7], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[155], Y[1], '+', locals[6]));
  algo.push_back(expr_t(locals[156], Y[1], '-', locals[6]));
  algo.push_back(expr_t(locals[157], Y[3], '-', locals[8]));
  algo.push_back(expr_t(locals[158], Y[3], '+', locals[8]));
  algo.push_back(expr_t(Y[1], locals[155]));
  algo.push_back(expr_t(Y[3], locals[157]));
  algo.push_back(expr_t(Y[5], locals[156]));
  algo.push_back(expr_t(Y[7], locals[158]));
  algo.push_back(expr_t(Y[8], X[2], '+', X[18]));
  algo.push_back(expr_t(Y[9], X[2], '-', X[18]));
  algo.push_back(expr_t(Y[10], X[10]));
  algo.push_back(expr_t(Y[11], X[26]));
  algo.push_back(expr_t(locals[9], Y[10], '+', Y[11]));
  algo.push_back(expr_t(locals[10], Y[10], '-', Y[11]));
  algo.push_back(expr_t(locals[11], locals[10], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[159], Y[8], '+', locals[9]));
  algo.push_back(expr_t(locals[160], Y[8], '-', locals[9]));
  algo.push_back(expr_t(locals[161], Y[9], '-', locals[11]));
  algo.push_back(expr_t(locals[162], Y[9], '+', locals[11]));
  algo.push_back(expr_t(Y[8], locals[159]));
  algo.push_back(expr_t(Y[9], locals[161]));
  algo.push_back(expr_t(Y[10], locals[160]));
  algo.push_back(expr_t(Y[11], locals[162]));
  algo.push_back(expr_t(Y[12], X[6], '+', X[22]));
  algo.push_back(expr_t(Y[13], X[6], '-', X[22]));
  algo.push_back(expr_t(Y[14], X[14]));
  algo.push_back(expr_t(Y[15], X[30]));
  algo.push_back(expr_t(locals[12], Y[14], '+', Y[15]));
  algo.push_back(expr_t(locals[13], Y[14], '-', Y[15]));
  algo.push_back(expr_t(locals[14], locals[13], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[163], Y[12], '+', locals[12]));
  algo.push_back(expr_t(locals[164], Y[12], '-', locals[12]));
  algo.push_back(expr_t(locals[165], Y[13], '-', locals[14]));
  algo.push_back(expr_t(locals[166], Y[13], '+', locals[14]));
  algo.push_back(expr_t(Y[12], locals[163]));
  algo.push_back(expr_t(Y[13], locals[165]));
  algo.push_back(expr_t(Y[14], locals[164]));
  algo.push_back(expr_t(Y[15], locals[166]));
  algo.push_back(expr_t(locals[15], Y[8], '+', Y[12]));
  algo.push_back(expr_t(locals[16], Y[8], '-', Y[12]));
  algo.push_back(expr_t(locals[17], locals[16], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[167], Y[0], '+', locals[15]));
  algo.push_back(expr_t(locals[168], Y[0], '-', locals[15]));
  algo.push_back(expr_t(locals[169], Y[4], '-', locals[17]));
  algo.push_back(expr_t(locals[170], Y[4], '+', locals[17]));
  algo.push_back(expr_t(Y[0], locals[167]));
  algo.push_back(expr_t(Y[4], locals[169]));
  algo.push_back(expr_t(Y[8], locals[168]));
  algo.push_back(expr_t(Y[12], locals[170]));
  algo.push_back(expr_t(locals[75], Y[9], '*',
                        atom(RT_REAL_CONST, 0.923879532511286738483136105060)));
  algo.push_back(
      expr_t(locals[76], Y[9], '*',
             atom(RT_IMAG_CONST, -0.382683432365089781779232680492)));
  algo.push_back(expr_t(locals[77], Y[13], '*',
                        atom(RT_REAL_CONST, 0.382683432365089837290383911750)));
  algo.push_back(
      expr_t(locals[78], Y[13], '*',
             atom(RT_IMAG_CONST, -0.923879532511286738483136105060)));
  algo.push_back(expr_t(locals[79], locals[75], '+', locals[76]));
  algo.push_back(expr_t(locals[80], locals[77], '+', locals[78]));
  algo.push_back(expr_t(locals[18], locals[79], '+', locals[80]));
  algo.push_back(expr_t(locals[19], locals[80], '-', locals[79]));
  algo.push_back(
      expr_t(locals[20], locals[19], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[171], Y[1], '+', locals[18]));
  algo.push_back(expr_t(locals[172], Y[1], '-', locals[18]));
  algo.push_back(expr_t(locals[173], Y[5], '-', locals[20]));
  algo.push_back(expr_t(locals[174], Y[5], '+', locals[20]));
  algo.push_back(expr_t(Y[1], locals[171]));
  algo.push_back(expr_t(Y[5], locals[173]));
  algo.push_back(expr_t(Y[9], locals[172]));
  algo.push_back(expr_t(Y[13], locals[174]));
  algo.push_back(expr_t(locals[81], Y[10], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[82], locals[81], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[83], Y[14], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[84], locals[83], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[85], locals[81], '+', locals[82]));
  algo.push_back(expr_t(locals[86], locals[83], '+', locals[84]));
  algo.push_back(expr_t(locals[21], locals[85], '+', locals[86]));
  algo.push_back(expr_t(locals[22], locals[86], '-', locals[85]));
  algo.push_back(
      expr_t(locals[23], locals[22], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[175], Y[2], '+', locals[21]));
  algo.push_back(expr_t(locals[176], Y[2], '-', locals[21]));
  algo.push_back(expr_t(locals[177], Y[6], '-', locals[23]));
  algo.push_back(expr_t(locals[178], Y[6], '+', locals[23]));
  algo.push_back(expr_t(Y[2], locals[175]));
  algo.push_back(expr_t(Y[6], locals[177]));
  algo.push_back(expr_t(Y[10], locals[176]));
  algo.push_back(expr_t(Y[14], locals[178]));
  algo.push_back(expr_t(locals[87], Y[11], '*',
                        atom(RT_REAL_CONST, 0.382683432365089837290383911750)));
  algo.push_back(
      expr_t(locals[88], Y[11], '*',
             atom(RT_IMAG_CONST, -0.923879532511286738483136105060)));
  algo.push_back(
      expr_t(locals[89], Y[15], '*',
             atom(RT_REAL_CONST, -0.923879532511286849505438567576)));
  algo.push_back(expr_t(locals[90], Y[15], '*',
                        atom(RT_IMAG_CONST, 0.382683432365089670756930217976)));
  algo.push_back(expr_t(locals[91], locals[87], '+', locals[88]));
  algo.push_back(expr_t(locals[92], locals[89], '+', locals[90]));
  algo.push_back(expr_t(locals[24], locals[91], '+', locals[92]));
  algo.push_back(expr_t(locals[25], locals[92], '-', locals[91]));
  algo.push_back(
      expr_t(locals[26], locals[25], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[179], Y[3], '+', locals[24]));
  algo.push_back(expr_t(locals[180], Y[3], '-', locals[24]));
  algo.push_back(expr_t(locals[181], Y[7], '-', locals[26]));
  algo.push_back(expr_t(locals[182], Y[7], '+', locals[26]));
  algo.push_back(expr_t(Y[3], locals[179]));
  algo.push_back(expr_t(Y[7], locals[181]));
  algo.push_back(expr_t(Y[11], locals[180]));
  algo.push_back(expr_t(Y[15], locals[182]));
  algo.push_back(expr_t(Y[16], X[1], '+', X[17]));
  algo.push_back(expr_t(Y[17], X[1], '-', X[17]));
  algo.push_back(expr_t(Y[18], X[9]));
  algo.push_back(expr_t(Y[19], X[25]));
  algo.push_back(expr_t(locals[27], Y[18], '+', Y[19]));
  algo.push_back(expr_t(locals[28], Y[18], '-', Y[19]));
  algo.push_back(expr_t(locals[29], locals[28], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[183], Y[16], '+', locals[27]));
  algo.push_back(expr_t(locals[184], Y[16], '-', locals[27]));
  algo.push_back(expr_t(locals[185], Y[17], '-', locals[29]));
  algo.push_back(expr_t(locals[186], Y[17], '+', locals[29]));
  algo.push_back(expr_t(Y[16], locals[183]));
  algo.push_back(expr_t(Y[17], locals[185]));
  algo.push_back(expr_t(Y[18], locals[184]));
  algo.push_back(expr_t(Y[19], locals[186]));
  algo.push_back(expr_t(Y[20], X[5], '+', X[21]));
  algo.push_back(expr_t(Y[21], X[5], '-', X[21]));
  algo.push_back(expr_t(Y[22], X[13], '+', X[29]));
  algo.push_back(expr_t(Y[23], X[13], '-', X[29]));
  algo.push_back(expr_t(locals[30], Y[20], '+', Y[22]));
  algo.push_back(expr_t(locals[31], Y[20], '-', Y[22]));
  algo.push_back(expr_t(locals[32], locals[31], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[187], Y[16], '+', locals[30]));
  algo.push_back(expr_t(locals[188], Y[16], '-', locals[30]));
  algo.push_back(expr_t(locals[189], Y[18], '-', locals[32]));
  algo.push_back(expr_t(locals[190], Y[18], '+', locals[32]));
  algo.push_back(expr_t(Y[16], locals[187]));
  algo.push_back(expr_t(Y[18], locals[189]));
  algo.push_back(expr_t(Y[20], locals[188]));
  algo.push_back(expr_t(Y[22], locals[190]));
  algo.push_back(expr_t(locals[93], Y[21], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[94], locals[93], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[95], Y[23], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[96], locals[95], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[97], locals[93], '+', locals[94]));
  algo.push_back(expr_t(locals[98], locals[95], '+', locals[96]));
  algo.push_back(expr_t(locals[33], locals[97], '+', locals[98]));
  algo.push_back(expr_t(locals[34], locals[98], '-', locals[97]));
  algo.push_back(
      expr_t(locals[35], locals[34], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[191], Y[17], '+', locals[33]));
  algo.push_back(expr_t(locals[192], Y[17], '-', locals[33]));
  algo.push_back(expr_t(locals[193], Y[19], '-', locals[35]));
  algo.push_back(expr_t(locals[194], Y[19], '+', locals[35]));
  algo.push_back(expr_t(Y[17], locals[191]));
  algo.push_back(expr_t(Y[19], locals[193]));
  algo.push_back(expr_t(Y[21], locals[192]));
  algo.push_back(expr_t(Y[23], locals[194]));
  algo.push_back(expr_t(Y[24], X[3], '+', X[19]));
  algo.push_back(expr_t(Y[25], X[3], '-', X[19]));
  algo.push_back(expr_t(Y[26], X[11]));
  algo.push_back(expr_t(Y[27], X[27]));
  algo.push_back(expr_t(locals[36], Y[26], '+', Y[27]));
  algo.push_back(expr_t(locals[37], Y[26], '-', Y[27]));
  algo.push_back(expr_t(locals[38], locals[37], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[195], Y[24], '+', locals[36]));
  algo.push_back(expr_t(locals[196], Y[24], '-', locals[36]));
  algo.push_back(expr_t(locals[197], Y[25], '-', locals[38]));
  algo.push_back(expr_t(locals[198], Y[25], '+', locals[38]));
  algo.push_back(expr_t(Y[24], locals[195]));
  algo.push_back(expr_t(Y[25], locals[197]));
  algo.push_back(expr_t(Y[26], locals[196]));
  algo.push_back(expr_t(Y[27], locals[198]));
  algo.push_back(expr_t(Y[28], X[7], '+', X[23]));
  algo.push_back(expr_t(Y[29], X[7], '-', X[23]));
  algo.push_back(expr_t(Y[30], X[15], '+', X[31]));
  algo.push_back(expr_t(Y[31], X[15], '-', X[31]));
  algo.push_back(expr_t(locals[39], Y[28], '+', Y[30]));
  algo.push_back(expr_t(locals[40], Y[28], '-', Y[30]));
  algo.push_back(expr_t(locals[41], locals[40], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[199], Y[24], '+', locals[39]));
  algo.push_back(expr_t(locals[200], Y[24], '-', locals[39]));
  algo.push_back(expr_t(locals[201], Y[26], '-', locals[41]));
  algo.push_back(expr_t(locals[202], Y[26], '+', locals[41]));
  algo.push_back(expr_t(Y[24], locals[199]));
  algo.push_back(expr_t(Y[26], locals[201]));
  algo.push_back(expr_t(Y[28], locals[200]));
  algo.push_back(expr_t(Y[30], locals[202]));
  algo.push_back(expr_t(locals[99], Y[29], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[100], locals[99], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[101], Y[31], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[102], locals[101], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[103], locals[99], '+', locals[100]));
  algo.push_back(expr_t(locals[104], locals[101], '+', locals[102]));
  algo.push_back(expr_t(locals[42], locals[103], '+', locals[104]));
  algo.push_back(expr_t(locals[43], locals[104], '-', locals[103]));
  algo.push_back(
      expr_t(locals[44], locals[43], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[203], Y[25], '+', locals[42]));
  algo.push_back(expr_t(locals[204], Y[25], '-', locals[42]));
  algo.push_back(expr_t(locals[205], Y[27], '-', locals[44]));
  algo.push_back(expr_t(locals[206], Y[27], '+', locals[44]));
  algo.push_back(expr_t(Y[25], locals[203]));
  algo.push_back(expr_t(Y[27], locals[205]));
  algo.push_back(expr_t(Y[29], locals[204]));
  algo.push_back(expr_t(Y[31], locals[206]));
  algo.push_back(expr_t(locals[45], Y[16], '+', Y[24]));
  algo.push_back(expr_t(locals[46], Y[16], '-', Y[24]));
  algo.push_back(expr_t(locals[47], locals[46], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[207], Y[0], '+', locals[45]));
  algo.push_back(expr_t(locals[208], Y[0], '-', locals[45]));
  algo.push_back(expr_t(locals[209], Y[8], '-', locals[47]));
  algo.push_back(expr_t(locals[210], Y[8], '+', locals[47]));
  algo.push_back(expr_t(Y[0], locals[207]));
  algo.push_back(expr_t(Y[8], locals[209]));
  algo.push_back(expr_t(Y[16], locals[208]));
  algo.push_back(expr_t(Y[24], locals[210]));
  algo.push_back(expr_t(locals[105], Y[17], '*',
                        atom(RT_REAL_CONST, 0.980785280403230430579242238309)));
  algo.push_back(
      expr_t(locals[106], Y[17], '*',
             atom(RT_IMAG_CONST, -0.195090322016128248083788321310)));
  algo.push_back(expr_t(locals[107], Y[25], '*',
                        atom(RT_REAL_CONST, 0.831469612302545235671402679145)));
  algo.push_back(
      expr_t(locals[108], Y[25], '*',
             atom(RT_IMAG_CONST, -0.555570233019602177648721408332)));
  algo.push_back(expr_t(locals[109], locals[105], '+', locals[106]));
  algo.push_back(expr_t(locals[110], locals[107], '+', locals[108]));
  algo.push_back(expr_t(locals[48], locals[109], '+', locals[110]));
  algo.push_back(expr_t(locals[49], locals[110], '-', locals[109]));
  algo.push_back(
      expr_t(locals[50], locals[49], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[211], Y[1], '+', locals[48]));
  algo.push_back(expr_t(locals[212], Y[1], '-', locals[48]));
  algo.push_back(expr_t(locals[213], Y[9], '-', locals[50]));
  algo.push_back(expr_t(locals[214], Y[9], '+', locals[50]));
  algo.push_back(expr_t(Y[1], locals[211]));
  algo.push_back(expr_t(Y[9], locals[213]));
  algo.push_back(expr_t(Y[17], locals[212]));
  algo.push_back(expr_t(Y[25], locals[214]));
  algo.push_back(expr_t(locals[111], Y[18], '*',
                        atom(RT_REAL_CONST, 0.923879532511286738483136105060)));
  algo.push_back(
      expr_t(locals[112], Y[18], '*',
             atom(RT_IMAG_CONST, -0.382683432365089781779232680492)));
  algo.push_back(expr_t(locals[113], Y[26], '*',
                        atom(RT_REAL_CONST, 0.382683432365089837290383911750)));
  algo.push_back(
      expr_t(locals[114], Y[26], '*',
             atom(RT_IMAG_CONST, -0.923879532511286738483136105060)));
  algo.push_back(expr_t(locals[115], locals[111], '+', locals[112]));
  algo.push_back(expr_t(locals[116], locals[113], '+', locals[114]));
  algo.push_back(expr_t(locals[51], locals[115], '+', locals[116]));
  algo.push_back(expr_t(locals[52], locals[116], '-', locals[115]));
  algo.push_back(
      expr_t(locals[53], locals[52], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[215], Y[2], '+', locals[51]));
  algo.push_back(expr_t(locals[216], Y[2], '-', locals[51]));
  algo.push_back(expr_t(locals[217], Y[10], '-', locals[53]));
  algo.push_back(expr_t(locals[218], Y[10], '+', locals[53]));
  algo.push_back(expr_t(Y[2], locals[215]));
  algo.push_back(expr_t(Y[10], locals[217]));
  algo.push_back(expr_t(Y[18], locals[216]));
  algo.push_back(expr_t(Y[26], locals[218]));
  algo.push_back(expr_t(locals[117], Y[19], '*',
                        atom(RT_REAL_CONST, 0.831469612302545235671402679145)));
  algo.push_back(
      expr_t(locals[118], Y[19], '*',
             atom(RT_IMAG_CONST, -0.555570233019602177648721408332)));
  algo.push_back(
      expr_t(locals[119], Y[27], '*',
             atom(RT_REAL_CONST, -0.195090322016128192572637090052)));
  algo.push_back(
      expr_t(locals[120], Y[27], '*',
             atom(RT_IMAG_CONST, -0.980785280403230430579242238309)));
  algo.push_back(expr_t(locals[121], locals[117], '+', locals[118]));
  algo.push_back(expr_t(locals[122], locals[119], '+', locals[120]));
  algo.push_back(expr_t(locals[54], locals[121], '+', locals[122]));
  algo.push_back(expr_t(locals[55], locals[122], '-', locals[121]));
  algo.push_back(
      expr_t(locals[56], locals[55], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[219], Y[3], '+', locals[54]));
  algo.push_back(expr_t(locals[220], Y[3], '-', locals[54]));
  algo.push_back(expr_t(locals[221], Y[11], '-', locals[56]));
  algo.push_back(expr_t(locals[222], Y[11], '+', locals[56]));
  algo.push_back(expr_t(Y[3], locals[219]));
  algo.push_back(expr_t(Y[11], locals[221]));
  algo.push_back(expr_t(Y[19], locals[220]));
  algo.push_back(expr_t(Y[27], locals[222]));
  algo.push_back(expr_t(locals[123], Y[20], '*',
                        atom(RT_REAL_CONST, 0.707106781186547572737310929369)));
  algo.push_back(
      expr_t(locals[124], locals[123], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[125], Y[28], '*',
             atom(RT_REAL_CONST, -0.707106781186547461715008466854)));
  algo.push_back(expr_t(locals[126], locals[125], '*',
                        atom(RT_IMAG_CONST, 1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[127], locals[123], '+', locals[124]));
  algo.push_back(expr_t(locals[128], locals[125], '+', locals[126]));
  algo.push_back(expr_t(locals[57], locals[127], '+', locals[128]));
  algo.push_back(expr_t(locals[58], locals[128], '-', locals[127]));
  algo.push_back(
      expr_t(locals[59], locals[58], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[223], Y[4], '+', locals[57]));
  algo.push_back(expr_t(locals[224], Y[4], '-', locals[57]));
  algo.push_back(expr_t(locals[225], Y[12], '-', locals[59]));
  algo.push_back(expr_t(locals[226], Y[12], '+', locals[59]));
  algo.push_back(expr_t(Y[4], locals[223]));
  algo.push_back(expr_t(Y[12], locals[225]));
  algo.push_back(expr_t(Y[20], locals[224]));
  algo.push_back(expr_t(Y[28], locals[226]));
  algo.push_back(expr_t(locals[129], Y[21], '*',
                        atom(RT_REAL_CONST, 0.555570233019602288671023870847)));
  algo.push_back(
      expr_t(locals[130], Y[21], '*',
             atom(RT_IMAG_CONST, -0.831469612302545235671402679145)));
  algo.push_back(
      expr_t(locals[131], Y[29], '*',
             atom(RT_REAL_CONST, -0.980785280403230430579242238309)));
  algo.push_back(
      expr_t(locals[132], Y[29], '*',
             atom(RT_IMAG_CONST, -0.195090322016128608906271324486)));
  algo.push_back(expr_t(locals[133], locals[129], '+', locals[130]));
  algo.push_back(expr_t(locals[134], locals[131], '+', locals[132]));
  algo.push_back(expr_t(locals[60], locals[133], '+', locals[134]));
  algo.push_back(expr_t(locals[61], locals[134], '-', locals[133]));
  algo.push_back(
      expr_t(locals[62], locals[61], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[227], Y[5], '+', locals[60]));
  algo.push_back(expr_t(locals[228], Y[5], '-', locals[60]));
  algo.push_back(expr_t(locals[229], Y[13], '-', locals[62]));
  algo.push_back(expr_t(locals[230], Y[13], '+', locals[62]));
  algo.push_back(expr_t(Y[5], locals[227]));
  algo.push_back(expr_t(Y[13], locals[229]));
  algo.push_back(expr_t(Y[21], locals[228]));
  algo.push_back(expr_t(Y[29], locals[230]));
  algo.push_back(expr_t(locals[135], Y[22], '*',
                        atom(RT_REAL_CONST, 0.382683432365089837290383911750)));
  algo.push_back(
      expr_t(locals[136], Y[22], '*',
             atom(RT_IMAG_CONST, -0.923879532511286738483136105060)));
  algo.push_back(
      expr_t(locals[137], Y[30], '*',
             atom(RT_REAL_CONST, -0.923879532511286849505438567576)));
  algo.push_back(expr_t(locals[138], Y[30], '*',
                        atom(RT_IMAG_CONST, 0.382683432365089670756930217976)));
  algo.push_back(expr_t(locals[139], locals[135], '+', locals[136]));
  algo.push_back(expr_t(locals[140], locals[137], '+', locals[138]));
  algo.push_back(expr_t(locals[63], locals[139], '+', locals[140]));
  algo.push_back(expr_t(locals[64], locals[140], '-', locals[139]));
  algo.push_back(
      expr_t(locals[65], locals[64], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[231], Y[6], '+', locals[63]));
  algo.push_back(expr_t(locals[232], Y[6], '-', locals[63]));
  algo.push_back(expr_t(locals[233], Y[14], '-', locals[65]));
  algo.push_back(expr_t(locals[234], Y[14], '+', locals[65]));
  algo.push_back(expr_t(Y[6], locals[231]));
  algo.push_back(expr_t(Y[14], locals[233]));
  algo.push_back(expr_t(Y[22], locals[232]));
  algo.push_back(expr_t(Y[30], locals[234]));
  algo.push_back(expr_t(locals[141], Y[23], '*',
                        atom(RT_REAL_CONST, 0.195090322016128331350515168197)));
  algo.push_back(
      expr_t(locals[142], Y[23], '*',
             atom(RT_IMAG_CONST, -0.980785280403230430579242238309)));
  algo.push_back(
      expr_t(locals[143], Y[31], '*',
             atom(RT_REAL_CONST, -0.555570233019602177648721408332)));
  algo.push_back(expr_t(locals[144], Y[31], '*',
                        atom(RT_IMAG_CONST, 0.831469612302545235671402679145)));
  algo.push_back(expr_t(locals[145], locals[141], '+', locals[142]));
  algo.push_back(expr_t(locals[146], locals[143], '+', locals[144]));
  algo.push_back(expr_t(locals[66], locals[145], '+', locals[146]));
  algo.push_back(expr_t(locals[67], locals[146], '-', locals[145]));
  algo.push_back(
      expr_t(locals[68], locals[67], '*',
             atom(RT_IMAG_CONST, -1.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[235], Y[7], '+', locals[66]));
  algo.push_back(expr_t(locals[236], Y[7], '-', locals[66]));
  algo.push_back(expr_t(locals[237], Y[15], '-', locals[68]));
  algo.push_back(expr_t(locals[238], Y[15], '+', locals[68]));
  algo.push_back(expr_t(Y[7], locals[235]));
  algo.push_back(expr_t(Y[15], locals[237]));
  algo.push_back(expr_t(Y[23], locals[236]));
  algo.push_back(expr_t(Y[31], locals[238]));
}

} // namespace plfft::wfta
