/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "wfta_nfact.hpp"
#include "algo.hpp"
#include "plfft_assert.hpp"
#include "winograd.hpp"

#include <complex>
#include <numeric>
#include <vector>

static int product(const std::vector<int> &nx) {
  return std::accumulate(nx.cbegin(), nx.cend(), 1, std::multiplies<int>());
}

namespace plfft::wfta {

static void wfta_nfact_rec(std::list<expr_t> &algo, atom *y, atom *x,
                           std::vector<int> nx, std::complex<double> C,
                           fresh_atom_factory &faf, bool use_alternative) {

  std::vector<std::complex<double>> Cn;
  void (*zn_in)(std::list<expr_t> &algo, atom *, const atom *, int,
                fresh_atom_factory &);
  void (*zn_out)(std::list<expr_t> &algo, atom *, const atom *, int,
                 fresh_atom_factory &);

  int n = nx.back();

  switch (n) {
  case 2:
    zn_in = &z2_in;
    zn_out = &z2_out;
    Cn = z2_mult();
    break;

  case 3:
    zn_in = &z3_in;
    zn_out = &z3_out;
    Cn = z3_mult();
    break;

  case 4:
    zn_in = &z4_in;
    zn_out = &z4_out;
    Cn = z4_mult();
    break;

  case 5:
    zn_in = &z5_in;
    zn_out = &z5_out;
    Cn = z5_mult();
    break;

  case 7:
    zn_in = &z7_in;
    zn_out = &z7_out;
    Cn = z7_mult();
    break;

  case 8:
    zn_in = &z8_in;
    zn_out = &z8_out;
    Cn = z8_mult();
    break;

  case 9:
    zn_in = &z9_in;
    zn_out = &z9_out;
    Cn = z9_mult();
    break;

  case 11:
    zn_in = &z11_in;
    zn_out = &z11_out;
    Cn = z11_mult();
    break;

  case 13:
    zn_in = &z13_in;
    zn_out = &z13_out;
    Cn = z13_mult();
    break;

  case 16:
    if (use_alternative) {
      split_radix_z16(algo, y, x, faf);
      return;
    }
    zn_in = &z16_in;
    zn_out = &z16_out;
    Cn = z16_mult();
    break;

  case 17:
    zn_in = &z17_in;
    zn_out = &z17_out;
    Cn = z17_mult();
    break;

  case 19:
    zn_in = &z19_in;
    zn_out = &z19_out;
    Cn = z19_mult();
    break;

  case 25:
    radix5_z25(algo, y, x, faf);
    return;

  case 32:
    split_radix_z32(algo, y, x, faf);
    return;

  default:
    ASSERT(false);
  }

  size_t mn = Cn.size();

  if (nx.size() == 1) {
    auto v = faf.get_many(mn);
    zn_in(algo, &v[0], x, 1, faf);

    for (size_t i = 0; i < mn; i++) {
      // This is either purely real, or purely imaginary
      std::complex<double> coeff = C * Cn[i];
      if (coeff.real() == 0.0) {
        algo.push_back(
            expr_t(v[i], v[i], '*', atom(RT_IMAG_CONST, coeff.imag())));
      } else {
        algo.push_back(
            expr_t(v[i], v[i], '*', atom(RT_REAL_CONST, coeff.real())));
      }
    }

    zn_out(algo, y, &v[0], 1, faf);
    return;
  }

  std::vector<int> nx_next(nx.cbegin(), --nx.cend());
  int nprod = product(nx_next);

  auto v = faf.get_many(mn * nprod);
  zn_in(algo, &v[0], x, nprod, faf);

  auto w = faf.get_many(mn * nprod);
  for (size_t i = 0; i < mn; i++) {
    wfta_nfact_rec(algo, &w[i * nprod], &v[i * nprod], nx_next, C * Cn[i], faf,
                   use_alternative);
  }

  zn_out(algo, y, &w[0], nprod, faf);
}

nfact_data wfta_nfact(std::vector<int> nx, bool use_alternative) {
  int n = product(nx);
  std::list<expr_t> algo;
  fresh_atom_factory faf;
  auto x = faf.get_many(n);
  auto y = faf.get_many(n);
  wfta_nfact_rec(algo, y.data(), x.data(), std::move(nx), 1.0, faf,
                 use_alternative);
  io_pointers iop{x[0].ival, x[0].ival + n, y[0].ival, y[0].ival + n,
                  faf().ival};
  return {std::move(algo), std::move(iop)};
}

} // namespace plfft::wfta
