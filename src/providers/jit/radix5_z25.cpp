/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "algo.hpp"
#include "winograd.hpp"

#include <vector>

namespace plfft::wfta {

void radix5_z25(std::list<expr_t> &algo, atom *Y, const atom *X,
                fresh_atom_factory &faf) {
  auto locals = faf.get_many(379);

  algo.push_back(expr_t(Y[0], X[0]));
  algo.push_back(expr_t(Y[1], X[5]));
  algo.push_back(expr_t(Y[2], X[10]));
  algo.push_back(expr_t(Y[3], X[15]));
  algo.push_back(expr_t(Y[4], X[20]));
  algo.push_back(expr_t(locals[0], Y[1], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[1], Y[1], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[2], locals[1], '+', locals[0]));
  algo.push_back(expr_t(locals[3], Y[2], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[4], Y[2], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[5], locals[4], '+', locals[3]));
  algo.push_back(expr_t(locals[6], Y[4], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[7], Y[4], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[8], locals[7], '+', locals[6]));
  algo.push_back(expr_t(locals[9], locals[2], '-', locals[8]));
  algo.push_back(expr_t(locals[10], locals[2], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[11], locals[10], '-', locals[9]));
  algo.push_back(expr_t(locals[12], Y[3], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[13], Y[3], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[14], locals[13], '+', locals[12]));
  algo.push_back(expr_t(locals[15], locals[5], '-', locals[14]));
  algo.push_back(expr_t(locals[16], locals[5], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[17], locals[16], '-', locals[15]));
  algo.push_back(expr_t(locals[18], locals[11], '+', locals[17]));
  algo.push_back(expr_t(locals[19], locals[11], '-', locals[17]));
  algo.push_back(expr_t(locals[20], locals[18], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[21], Y[0], '-', locals[20]));
  algo.push_back(expr_t(locals[22], locals[15], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[23], locals[9], '+', locals[22]));
  algo.push_back(expr_t(locals[24], locals[19], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[25], locals[21], '-', locals[24]));
  algo.push_back(expr_t(locals[26], locals[21], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[27], locals[26], '-', locals[25]));
  algo.push_back(expr_t(locals[28], locals[9], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[29], locals[28], '-', locals[15]));
  algo.push_back(expr_t(Y[0], Y[0], '+', locals[18]));
  algo.push_back(expr_t(locals[30], locals[23], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[31], locals[27], '-', locals[30]));
  algo.push_back(expr_t(Y[1], locals[31]));
  algo.push_back(expr_t(locals[32], locals[29], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[33], locals[25], '-', locals[32]));
  algo.push_back(expr_t(Y[2], locals[33]));
  algo.push_back(expr_t(locals[34], locals[25], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[3], locals[34], '-', locals[33]));
  algo.push_back(expr_t(locals[36], locals[27], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[4], locals[36], '-', locals[31]));
  algo.push_back(expr_t(Y[5], X[1]));
  algo.push_back(expr_t(Y[6], X[6]));
  algo.push_back(expr_t(Y[7], X[11]));
  algo.push_back(expr_t(Y[8], X[16]));
  algo.push_back(expr_t(Y[9], X[21]));
  algo.push_back(expr_t(locals[38], Y[6], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[39], Y[6], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[40], locals[39], '+', locals[38]));
  algo.push_back(expr_t(locals[41], Y[7], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[42], Y[7], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[43], locals[42], '+', locals[41]));
  algo.push_back(expr_t(locals[44], Y[9], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[45], Y[9], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[46], locals[45], '+', locals[44]));
  algo.push_back(expr_t(locals[47], locals[40], '-', locals[46]));
  algo.push_back(expr_t(locals[48], locals[40], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[49], locals[48], '-', locals[47]));
  algo.push_back(expr_t(locals[50], Y[8], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[51], Y[8], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[52], locals[51], '+', locals[50]));
  algo.push_back(expr_t(locals[53], locals[43], '-', locals[52]));
  algo.push_back(expr_t(locals[54], locals[43], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[55], locals[54], '-', locals[53]));
  algo.push_back(expr_t(locals[56], locals[49], '+', locals[55]));
  algo.push_back(expr_t(locals[57], locals[49], '-', locals[55]));
  algo.push_back(expr_t(locals[58], locals[56], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[59], Y[5], '-', locals[58]));
  algo.push_back(expr_t(locals[60], locals[53], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[61], locals[47], '+', locals[60]));
  algo.push_back(expr_t(locals[62], locals[57], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[63], locals[59], '-', locals[62]));
  algo.push_back(expr_t(locals[64], locals[59], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[65], locals[64], '-', locals[63]));
  algo.push_back(expr_t(locals[66], locals[47], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[67], locals[66], '-', locals[53]));
  algo.push_back(expr_t(Y[5], Y[5], '+', locals[56]));
  algo.push_back(expr_t(locals[68], locals[61], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[69], locals[65], '-', locals[68]));
  algo.push_back(expr_t(Y[6], locals[69]));
  algo.push_back(expr_t(locals[70], locals[67], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[71], locals[63], '-', locals[70]));
  algo.push_back(expr_t(Y[7], locals[71]));
  algo.push_back(expr_t(locals[72], locals[63], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[8], locals[72], '-', locals[71]));
  algo.push_back(expr_t(locals[74], locals[65], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[9], locals[74], '-', locals[69]));
  algo.push_back(expr_t(Y[10], X[2]));
  algo.push_back(expr_t(Y[11], X[7]));
  algo.push_back(expr_t(Y[12], X[12]));
  algo.push_back(expr_t(Y[13], X[17]));
  algo.push_back(expr_t(Y[14], X[22]));
  algo.push_back(expr_t(locals[76], Y[11], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[77], Y[11], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[78], locals[77], '+', locals[76]));
  algo.push_back(expr_t(locals[79], Y[12], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[80], Y[12], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[81], locals[80], '+', locals[79]));
  algo.push_back(expr_t(locals[82], Y[14], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[83], Y[14], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[84], locals[83], '+', locals[82]));
  algo.push_back(expr_t(locals[85], locals[78], '-', locals[84]));
  algo.push_back(expr_t(locals[86], locals[78], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[87], locals[86], '-', locals[85]));
  algo.push_back(expr_t(locals[88], Y[13], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[89], Y[13], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[90], locals[89], '+', locals[88]));
  algo.push_back(expr_t(locals[91], locals[81], '-', locals[90]));
  algo.push_back(expr_t(locals[92], locals[81], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[93], locals[92], '-', locals[91]));
  algo.push_back(expr_t(locals[94], locals[87], '+', locals[93]));
  algo.push_back(expr_t(locals[95], locals[87], '-', locals[93]));
  algo.push_back(expr_t(locals[96], locals[94], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[97], Y[10], '-', locals[96]));
  algo.push_back(expr_t(locals[98], locals[91], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[99], locals[85], '+', locals[98]));
  algo.push_back(expr_t(locals[100], locals[95], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[101], locals[97], '-', locals[100]));
  algo.push_back(expr_t(locals[102], locals[97], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[103], locals[102], '-', locals[101]));
  algo.push_back(expr_t(locals[104], locals[85], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[105], locals[104], '-', locals[91]));
  algo.push_back(expr_t(Y[10], Y[10], '+', locals[94]));
  algo.push_back(expr_t(locals[106], locals[99], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[107], locals[103], '-', locals[106]));
  algo.push_back(expr_t(Y[11], locals[107]));
  algo.push_back(expr_t(locals[108], locals[105], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[109], locals[101], '-', locals[108]));
  algo.push_back(expr_t(Y[12], locals[109]));
  algo.push_back(expr_t(locals[110], locals[101], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[13], locals[110], '-', locals[109]));
  algo.push_back(expr_t(locals[112], locals[103], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[14], locals[112], '-', locals[107]));
  algo.push_back(expr_t(Y[15], X[3]));
  algo.push_back(expr_t(Y[16], X[8]));
  algo.push_back(expr_t(Y[17], X[13]));
  algo.push_back(expr_t(Y[18], X[18]));
  algo.push_back(expr_t(Y[19], X[23]));
  algo.push_back(expr_t(locals[114], Y[16], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[115], Y[16], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[116], locals[115], '+', locals[114]));
  algo.push_back(expr_t(locals[117], Y[17], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[118], Y[17], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[119], locals[118], '+', locals[117]));
  algo.push_back(expr_t(locals[120], Y[19], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[121], Y[19], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[122], locals[121], '+', locals[120]));
  algo.push_back(expr_t(locals[123], locals[116], '-', locals[122]));
  algo.push_back(expr_t(locals[124], locals[116], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[125], locals[124], '-', locals[123]));
  algo.push_back(expr_t(locals[126], Y[18], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[127], Y[18], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[128], locals[127], '+', locals[126]));
  algo.push_back(expr_t(locals[129], locals[119], '-', locals[128]));
  algo.push_back(expr_t(locals[130], locals[119], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[131], locals[130], '-', locals[129]));
  algo.push_back(expr_t(locals[132], locals[125], '+', locals[131]));
  algo.push_back(expr_t(locals[133], locals[125], '-', locals[131]));
  algo.push_back(expr_t(locals[134], locals[132], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[135], Y[15], '-', locals[134]));
  algo.push_back(expr_t(locals[136], locals[129], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[137], locals[123], '+', locals[136]));
  algo.push_back(expr_t(locals[138], locals[133], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[139], locals[135], '-', locals[138]));
  algo.push_back(expr_t(locals[140], locals[135], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[141], locals[140], '-', locals[139]));
  algo.push_back(expr_t(locals[142], locals[123], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[143], locals[142], '-', locals[129]));
  algo.push_back(expr_t(Y[15], Y[15], '+', locals[132]));
  algo.push_back(expr_t(locals[144], locals[137], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[145], locals[141], '-', locals[144]));
  algo.push_back(expr_t(Y[16], locals[145]));
  algo.push_back(expr_t(locals[146], locals[143], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[147], locals[139], '-', locals[146]));
  algo.push_back(expr_t(Y[17], locals[147]));
  algo.push_back(expr_t(locals[148], locals[139], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[18], locals[148], '-', locals[147]));
  algo.push_back(expr_t(locals[150], locals[141], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[19], locals[150], '-', locals[145]));
  algo.push_back(expr_t(Y[20], X[4]));
  algo.push_back(expr_t(Y[21], X[9]));
  algo.push_back(expr_t(Y[22], X[14]));
  algo.push_back(expr_t(Y[23], X[19]));
  algo.push_back(expr_t(Y[24], X[24]));
  algo.push_back(expr_t(locals[152], Y[21], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[153], Y[21], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[154], locals[153], '+', locals[152]));
  algo.push_back(expr_t(locals[155], Y[22], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[156], Y[22], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[157], locals[156], '+', locals[155]));
  algo.push_back(expr_t(locals[158], Y[24], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[159], Y[24], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[160], locals[159], '+', locals[158]));
  algo.push_back(expr_t(locals[161], locals[154], '-', locals[160]));
  algo.push_back(expr_t(locals[162], locals[154], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[163], locals[162], '-', locals[161]));
  algo.push_back(expr_t(locals[164], Y[23], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[165], Y[23], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[166], locals[165], '+', locals[164]));
  algo.push_back(expr_t(locals[167], locals[157], '-', locals[166]));
  algo.push_back(expr_t(locals[168], locals[157], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[169], locals[168], '-', locals[167]));
  algo.push_back(expr_t(locals[170], locals[163], '+', locals[169]));
  algo.push_back(expr_t(locals[171], locals[163], '-', locals[169]));
  algo.push_back(expr_t(locals[172], locals[170], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[173], Y[20], '-', locals[172]));
  algo.push_back(expr_t(locals[174], locals[167], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[175], locals[161], '+', locals[174]));
  algo.push_back(expr_t(locals[176], locals[171], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[177], locals[173], '-', locals[176]));
  algo.push_back(expr_t(locals[178], locals[173], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[179], locals[178], '-', locals[177]));
  algo.push_back(expr_t(locals[180], locals[161], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[181], locals[180], '-', locals[167]));
  algo.push_back(expr_t(Y[20], Y[20], '+', locals[170]));
  algo.push_back(expr_t(locals[182], locals[175], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[183], locals[179], '-', locals[182]));
  algo.push_back(expr_t(Y[21], locals[183]));
  algo.push_back(expr_t(locals[184], locals[181], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[185], locals[177], '-', locals[184]));
  algo.push_back(expr_t(Y[22], locals[185]));
  algo.push_back(expr_t(locals[186], locals[177], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[23], locals[186], '-', locals[185]));
  algo.push_back(expr_t(locals[188], locals[179], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[24], locals[188], '-', locals[183]));
  algo.push_back(expr_t(locals[190], Y[5], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[191], Y[5], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[192], locals[191], '+', locals[190]));
  algo.push_back(expr_t(locals[193], Y[10], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[194], Y[10], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[195], locals[194], '+', locals[193]));
  algo.push_back(expr_t(locals[196], Y[20], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[197], Y[20], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[198], locals[197], '+', locals[196]));
  algo.push_back(expr_t(locals[199], locals[192], '-', locals[198]));
  algo.push_back(expr_t(locals[200], locals[192], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[201], locals[200], '-', locals[199]));
  algo.push_back(expr_t(locals[202], Y[15], '*',
                        atom(RT_REAL_CONST, 1.000000000000000000000000000000)));
  algo.push_back(
      expr_t(locals[203], Y[15], '*',
             atom(RT_IMAG_CONST, -0.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[204], locals[203], '+', locals[202]));
  algo.push_back(expr_t(locals[205], locals[195], '-', locals[204]));
  algo.push_back(expr_t(locals[206], locals[195], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[207], locals[206], '-', locals[205]));
  algo.push_back(expr_t(locals[208], locals[201], '+', locals[207]));
  algo.push_back(expr_t(locals[209], locals[201], '-', locals[207]));
  algo.push_back(expr_t(locals[210], locals[208], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[211], Y[0], '-', locals[210]));
  algo.push_back(expr_t(locals[212], locals[205], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[213], locals[199], '+', locals[212]));
  algo.push_back(expr_t(locals[214], locals[209], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[215], locals[211], '-', locals[214]));
  algo.push_back(expr_t(locals[216], locals[211], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[217], locals[216], '-', locals[215]));
  algo.push_back(expr_t(locals[218], locals[199], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[219], locals[218], '-', locals[205]));
  algo.push_back(expr_t(Y[0], Y[0], '+', locals[208]));
  algo.push_back(expr_t(locals[220], locals[213], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[221], locals[217], '-', locals[220]));
  algo.push_back(expr_t(Y[5], locals[221]));
  algo.push_back(expr_t(locals[222], locals[219], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[223], locals[215], '-', locals[222]));
  algo.push_back(expr_t(Y[10], locals[223]));
  algo.push_back(expr_t(locals[224], locals[215], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[15], locals[224], '-', locals[223]));
  algo.push_back(expr_t(locals[226], locals[217], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[20], locals[226], '-', locals[221]));
  algo.push_back(expr_t(locals[228], Y[6], '*',
                        atom(RT_REAL_CONST, 0.968583161128631076053352444433)));
  algo.push_back(
      expr_t(locals[229], Y[6], '*',
             atom(RT_IMAG_CONST, -0.248689887164854794843193985798)));
  algo.push_back(expr_t(locals[230], locals[229], '+', locals[228]));
  algo.push_back(expr_t(locals[231], Y[11], '*',
                        atom(RT_REAL_CONST, 0.876306680043863583939867112349)));
  algo.push_back(
      expr_t(locals[232], Y[11], '*',
             atom(RT_IMAG_CONST, -0.481753674101715323452310713037)));
  algo.push_back(expr_t(locals[233], locals[232], '+', locals[231]));
  algo.push_back(expr_t(locals[234], Y[21], '*',
                        atom(RT_REAL_CONST, 0.535826794978996545637528470252)));
  algo.push_back(
      expr_t(locals[235], Y[21], '*',
             atom(RT_IMAG_CONST, -0.844327925502015075309714120522)));
  algo.push_back(expr_t(locals[236], locals[235], '+', locals[234]));
  algo.push_back(expr_t(locals[237], locals[230], '-', locals[236]));
  algo.push_back(expr_t(locals[238], locals[230], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[239], locals[238], '-', locals[237]));
  algo.push_back(expr_t(locals[240], Y[16], '*',
                        atom(RT_REAL_CONST, 0.728968627421411552447239046160)));
  algo.push_back(
      expr_t(locals[241], Y[16], '*',
             atom(RT_IMAG_CONST, -0.684547105928688615072985612642)));
  algo.push_back(expr_t(locals[242], locals[241], '+', locals[240]));
  algo.push_back(expr_t(locals[243], locals[233], '-', locals[242]));
  algo.push_back(expr_t(locals[244], locals[233], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[245], locals[244], '-', locals[243]));
  algo.push_back(expr_t(locals[246], locals[239], '+', locals[245]));
  algo.push_back(expr_t(locals[247], locals[239], '-', locals[245]));
  algo.push_back(expr_t(locals[248], locals[246], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[249], Y[1], '-', locals[248]));
  algo.push_back(expr_t(locals[250], locals[243], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[251], locals[237], '+', locals[250]));
  algo.push_back(expr_t(locals[252], locals[247], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[253], locals[249], '-', locals[252]));
  algo.push_back(expr_t(locals[254], locals[249], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[255], locals[254], '-', locals[253]));
  algo.push_back(expr_t(locals[256], locals[237], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[257], locals[256], '-', locals[243]));
  algo.push_back(expr_t(Y[1], Y[1], '+', locals[246]));
  algo.push_back(expr_t(locals[258], locals[251], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[259], locals[255], '-', locals[258]));
  algo.push_back(expr_t(Y[6], locals[259]));
  algo.push_back(expr_t(locals[260], locals[257], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[261], locals[253], '-', locals[260]));
  algo.push_back(expr_t(Y[11], locals[261]));
  algo.push_back(expr_t(locals[262], locals[253], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[16], locals[262], '-', locals[261]));
  algo.push_back(expr_t(locals[264], locals[255], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[21], locals[264], '-', locals[259]));
  algo.push_back(expr_t(locals[266], Y[7], '*',
                        atom(RT_REAL_CONST, 0.876306680043863583939867112349)));
  algo.push_back(
      expr_t(locals[267], Y[7], '*',
             atom(RT_IMAG_CONST, -0.481753674101715323452310713037)));
  algo.push_back(expr_t(locals[268], locals[267], '+', locals[266]));
  algo.push_back(expr_t(locals[269], Y[12], '*',
                        atom(RT_REAL_CONST, 0.535826794978996545637528470252)));
  algo.push_back(
      expr_t(locals[270], Y[12], '*',
             atom(RT_IMAG_CONST, -0.844327925502015075309714120522)));
  algo.push_back(expr_t(locals[271], locals[270], '+', locals[269]));
  algo.push_back(
      expr_t(locals[272], Y[22], '*',
             atom(RT_REAL_CONST, -0.425779291565072715020079385795)));
  algo.push_back(
      expr_t(locals[273], Y[22], '*',
             atom(RT_IMAG_CONST, -0.904827052466019465803981347563)));
  algo.push_back(expr_t(locals[274], locals[273], '+', locals[272]));
  algo.push_back(expr_t(locals[275], locals[268], '-', locals[274]));
  algo.push_back(expr_t(locals[276], locals[268], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[277], locals[276], '-', locals[275]));
  algo.push_back(expr_t(locals[278], Y[17], '*',
                        atom(RT_REAL_CONST, 0.062790519529313526536640210907)));
  algo.push_back(
      expr_t(locals[279], Y[17], '*',
             atom(RT_IMAG_CONST, -0.998026728428271558968276622181)));
  algo.push_back(expr_t(locals[280], locals[279], '+', locals[278]));
  algo.push_back(expr_t(locals[281], locals[271], '-', locals[280]));
  algo.push_back(expr_t(locals[282], locals[271], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[283], locals[282], '-', locals[281]));
  algo.push_back(expr_t(locals[284], locals[277], '+', locals[283]));
  algo.push_back(expr_t(locals[285], locals[277], '-', locals[283]));
  algo.push_back(expr_t(locals[286], locals[284], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[287], Y[2], '-', locals[286]));
  algo.push_back(expr_t(locals[288], locals[281], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[289], locals[275], '+', locals[288]));
  algo.push_back(expr_t(locals[290], locals[285], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[291], locals[287], '-', locals[290]));
  algo.push_back(expr_t(locals[292], locals[287], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[293], locals[292], '-', locals[291]));
  algo.push_back(expr_t(locals[294], locals[275], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[295], locals[294], '-', locals[281]));
  algo.push_back(expr_t(Y[2], Y[2], '+', locals[284]));
  algo.push_back(expr_t(locals[296], locals[289], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[297], locals[293], '-', locals[296]));
  algo.push_back(expr_t(Y[7], locals[297]));
  algo.push_back(expr_t(locals[298], locals[295], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[299], locals[291], '-', locals[298]));
  algo.push_back(expr_t(Y[12], locals[299]));
  algo.push_back(expr_t(locals[300], locals[291], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[17], locals[300], '-', locals[299]));
  algo.push_back(expr_t(locals[302], locals[293], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[22], locals[302], '-', locals[297]));
  algo.push_back(expr_t(locals[304], Y[8], '*',
                        atom(RT_REAL_CONST, 0.728968627421411552447239046160)));
  algo.push_back(
      expr_t(locals[305], Y[8], '*',
             atom(RT_IMAG_CONST, -0.684547105928688615072985612642)));
  algo.push_back(expr_t(locals[306], locals[305], '+', locals[304]));
  algo.push_back(expr_t(locals[307], Y[13], '*',
                        atom(RT_REAL_CONST, 0.062790519529313526536640210907)));
  algo.push_back(
      expr_t(locals[308], Y[13], '*',
             atom(RT_IMAG_CONST, -0.998026728428271558968276622181)));
  algo.push_back(expr_t(locals[309], locals[308], '+', locals[307]));
  algo.push_back(
      expr_t(locals[310], Y[23], '*',
             atom(RT_REAL_CONST, -0.992114701314477764881871735270)));
  algo.push_back(
      expr_t(locals[311], Y[23], '*',
             atom(RT_IMAG_CONST, -0.125333233564304535878619617506)));
  algo.push_back(expr_t(locals[312], locals[311], '+', locals[310]));
  algo.push_back(expr_t(locals[313], locals[306], '-', locals[312]));
  algo.push_back(expr_t(locals[314], locals[306], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[315], locals[314], '-', locals[313]));
  algo.push_back(
      expr_t(locals[316], Y[18], '*',
             atom(RT_REAL_CONST, -0.637423989748689745482579382951)));
  algo.push_back(
      expr_t(locals[317], Y[18], '*',
             atom(RT_IMAG_CONST, -0.770513242775789253258267308411)));
  algo.push_back(expr_t(locals[318], locals[317], '+', locals[316]));
  algo.push_back(expr_t(locals[319], locals[309], '-', locals[318]));
  algo.push_back(expr_t(locals[320], locals[309], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[321], locals[320], '-', locals[319]));
  algo.push_back(expr_t(locals[322], locals[315], '+', locals[321]));
  algo.push_back(expr_t(locals[323], locals[315], '-', locals[321]));
  algo.push_back(expr_t(locals[324], locals[322], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[325], Y[3], '-', locals[324]));
  algo.push_back(expr_t(locals[326], locals[319], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[327], locals[313], '+', locals[326]));
  algo.push_back(expr_t(locals[328], locals[323], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[329], locals[325], '-', locals[328]));
  algo.push_back(expr_t(locals[330], locals[325], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[331], locals[330], '-', locals[329]));
  algo.push_back(expr_t(locals[332], locals[313], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[333], locals[332], '-', locals[319]));
  algo.push_back(expr_t(Y[3], Y[3], '+', locals[322]));
  algo.push_back(expr_t(locals[334], locals[327], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[335], locals[331], '-', locals[334]));
  algo.push_back(expr_t(Y[8], locals[335]));
  algo.push_back(expr_t(locals[336], locals[333], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[337], locals[329], '-', locals[336]));
  algo.push_back(expr_t(Y[13], locals[337]));
  algo.push_back(expr_t(locals[338], locals[329], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[18], locals[338], '-', locals[337]));
  algo.push_back(expr_t(locals[340], locals[331], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[23], locals[340], '-', locals[335]));
  algo.push_back(expr_t(locals[342], Y[9], '*',
                        atom(RT_REAL_CONST, 0.535826794978996545637528470252)));
  algo.push_back(
      expr_t(locals[343], Y[9], '*',
             atom(RT_IMAG_CONST, -0.844327925502015075309714120522)));
  algo.push_back(expr_t(locals[344], locals[343], '+', locals[342]));
  algo.push_back(
      expr_t(locals[345], Y[14], '*',
             atom(RT_REAL_CONST, -0.425779291565072715020079385795)));
  algo.push_back(
      expr_t(locals[346], Y[14], '*',
             atom(RT_IMAG_CONST, -0.904827052466019465803981347563)));
  algo.push_back(expr_t(locals[347], locals[346], '+', locals[345]));
  algo.push_back(
      expr_t(locals[348], Y[24], '*',
             atom(RT_REAL_CONST, -0.637423989748689523437974457920)));
  algo.push_back(expr_t(locals[349], Y[24], '*',
                        atom(RT_IMAG_CONST, 0.770513242775789364280569770926)));
  algo.push_back(expr_t(locals[350], locals[349], '+', locals[348]));
  algo.push_back(expr_t(locals[351], locals[344], '-', locals[350]));
  algo.push_back(expr_t(locals[352], locals[344], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[353], locals[352], '-', locals[351]));
  algo.push_back(
      expr_t(locals[354], Y[19], '*',
             atom(RT_REAL_CONST, -0.992114701314477764881871735270)));
  algo.push_back(
      expr_t(locals[355], Y[19], '*',
             atom(RT_IMAG_CONST, -0.125333233564304535878619617506)));
  algo.push_back(expr_t(locals[356], locals[355], '+', locals[354]));
  algo.push_back(expr_t(locals[357], locals[347], '-', locals[356]));
  algo.push_back(expr_t(locals[358], locals[347], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[359], locals[358], '-', locals[357]));
  algo.push_back(expr_t(locals[360], locals[353], '+', locals[359]));
  algo.push_back(expr_t(locals[361], locals[353], '-', locals[359]));
  algo.push_back(expr_t(locals[362], locals[360], '*',
                        atom(RT_REAL_CONST, 0.250000000000000000000000000000)));
  algo.push_back(expr_t(locals[363], Y[4], '-', locals[362]));
  algo.push_back(expr_t(locals[364], locals[357], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[365], locals[351], '+', locals[364]));
  algo.push_back(expr_t(locals[366], locals[361], '*',
                        atom(RT_REAL_CONST, 0.559016994374947451262869435595)));
  algo.push_back(expr_t(locals[367], locals[363], '-', locals[366]));
  algo.push_back(expr_t(locals[368], locals[363], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(locals[369], locals[368], '-', locals[367]));
  algo.push_back(expr_t(locals[370], locals[351], '*',
                        atom(RT_REAL_CONST, 0.618033988749894902525738871191)));
  algo.push_back(expr_t(locals[371], locals[370], '-', locals[357]));
  algo.push_back(expr_t(Y[4], Y[4], '+', locals[360]));
  algo.push_back(expr_t(locals[372], locals[365], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[373], locals[369], '-', locals[372]));
  algo.push_back(expr_t(Y[9], locals[373]));
  algo.push_back(expr_t(locals[374], locals[371], '*',
                        atom(RT_IMAG_CONST, 0.951056516295153531181938433292)));
  algo.push_back(expr_t(locals[375], locals[367], '-', locals[374]));
  algo.push_back(expr_t(Y[14], locals[375]));
  algo.push_back(expr_t(locals[376], locals[367], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[19], locals[376], '-', locals[375]));
  algo.push_back(expr_t(locals[378], locals[369], '*',
                        atom(RT_REAL_CONST, 2.000000000000000000000000000000)));
  algo.push_back(expr_t(Y[24], locals[378], '-', locals[373]));
}

} // namespace plfft::wfta
