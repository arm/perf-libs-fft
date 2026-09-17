/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "winograd_reorder.hpp"

#include <complex>

/*
 * Reference: An Introduction to Programming the Winograd Fourier Transform
 * (WFTA) by Harvey Silverman. IEEE Transactions on Acoustics, Speech and Signal
 * Processing, Vol. ASSP-25, No. 2, April 1977.
 *
 * See Section IV, part A: Mapping Vectors.
 *
 */

namespace plfft::wfta {

static int product(std::vector<int>::const_iterator begin,
                   std::vector<int>::const_iterator end) {
  int prod = 1;
  for (auto it = begin; it != end; ++it) {
    prod *= *it;
  }
  return prod;
}

static std::vector<int> generate_Hnl(int n, int nl) {
  std::vector<int> Hnl(nl);
  for (int j = 0; j < nl; j++) {
    for (int m = 0; m < nl; m++) {
      Hnl[j] = (m * n) / nl;
      if (Hnl[j] % nl == j) {
        break;
      }
    }
  }
  return Hnl;
}

static std::vector<int> generate_Hnl_prime(int n, int nl) {
  std::vector<int> Hnl_p(nl);
  for (int j = 0; j < nl; j++) {
    Hnl_p[j] = (j * n) / nl;
  }
  return Hnl_p;
}

static std::vector<int> get_ivals(const std::vector<int> &x, int i) {
  std::vector<int> y(x.size());
  for (size_t j = 0; j < x.size(); j++) {
    y[j] = i / product(x.cbegin(), x.cbegin() + j) % x[j];
  }
  return y;
}

static int V(const std::vector<int> &indices,
             const std::vector<std::vector<int>> &Hn_p) {
  int sum = 0;
  for (size_t i = 0; i < Hn_p.size(); i++) {
    sum += Hn_p[i][indices[i]];
  }
  return sum;
}

// Return the permutation vector, such that rtn[i] gives the
// index we actually need to read from, after reordering
std::vector<int64_t> get_in_perm(const std::vector<int> &nx) {
  int n = product(nx.cbegin(), nx.cend());

  std::vector<std::vector<int>> Hn_p;
  for (auto it = nx.cbegin(); it != nx.cend(); ++it) {
    Hn_p.push_back(generate_Hnl_prime(n, *it));
  }

  std::vector<int64_t> perm(n);
  for (int i = 0; i < n; i++) {
    const std::vector<int> indices = get_ivals(nx, i);
    perm[i] = V(indices, Hn_p) % n;
  }
  return perm;
}

// Return the permutation vector, such that rtn[i] gives the
// index we actually need to write to, after reordering
std::vector<int64_t> get_out_perm(const std::vector<int> &nx) {
  int n = product(nx.cbegin(), nx.cend());

  std::vector<std::vector<int>> Hn;
  for (auto it = nx.cbegin(); it != nx.cend(); ++it) {
    Hn.push_back(generate_Hnl(n, *it));
  }

  std::vector<int64_t> perm(n);
  for (int i = 0; i < n; i++) {
    const std::vector<int> indices = get_ivals(nx, i);
    perm[i] = V(indices, Hn) % n;
  }
  return perm;
}

} // end namespace plfft::wfta
