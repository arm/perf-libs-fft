/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include "kernel_data.hpp"
#include "plfft_util.hpp"

#include <cstdlib>

namespace plfft {

inline int64_t smallest_prime_factor(int64_t n) {
  if (n % 2 == 0) {
    return 2;
  }
  if (n % 3 == 0) {
    return 3;
  }

  // other than 2 and 3, all prime numbers are of the form 6k +/- 1
  int64_t d = 5;
  while (d <= n / d) {
    if (n % d == 0) {
      return d;
    }
    if (n % (d + 2) == 0) {
      return d + 2;
    }
    d += 6;
  }

  return n;
}

inline int64_t log_base(int64_t n, int64_t base) {
  return (int64_t)(std::ceil(std::log((double)n) / std::log((double)base)));
}

// This function is to be called after factorizing using the base case kernel
// lengths; n should be the product of primes by the time this function is
// called.
inline void factorize_primes(int64_t n, pod_vector<int64_t> &factors) {
  // Perform a prime factorization of the number that remains
  // once we have factorized using the lengths we have available
  for (int64_t factor = 23; factor < (int64_t)std::sqrt(n) + 1; factor += 2) {
    while (n % factor == 0) {
      n /= factor;
      factors.push_back(factor);
    }
  }

  // If we have anything left is it a prime number
  if (n > 1) {
    factors.push_back(n);
  }

  // Comparison function for a reverse qsort
  auto compar = [](const void *a, const void *b) -> int {
    auto aa = *(const int64_t *)a;
    auto bb = *(const int64_t *)b;
    return aa > bb ? -1 : aa == bb ? 0 : 1;
  };

  // Return the factorization sorted in descending order
  std::qsort(factors.begin(), factors.size(), sizeof(int64_t), compar);
}

// Look for the next size >= n which would allow us to use the given factors
// alone.
inline int64_t
next_length_factorable_by(int64_t n, const pod_vector<int> &kernel_factors) {
  int64_t length = n - 1;
  int64_t candidate;
  do {
    candidate = ++length;
    for (const auto factor : kernel_factors) {
      while (candidate % factor == 0) {
        candidate /= factor;
      }
    }
  } while (candidate != 1);
  return length;
}

namespace internal {
// 64 is a strict upper bound on the number of factors since
// the largest n we could be asked to factorize is 2^63 - 1
constexpr int max_factors = 64;
} // namespace internal

template<typename FloatTypeX, typename FloatTypeY>
pod_vector<int64_t> factorize_descending(int64_t n) {
  // Factorize using the kernel lengths we have available
  pod_vector<int64_t> factors;
  factors.reserve(internal::max_factors);
  pod_vector<int> k_facts = get_kernel_ns<FloatTypeX, FloatTypeY>();
  for (auto factor : k_facts) {
    while (n % factor == 0) {
      n /= factor;
      factors.push_back(factor);
    }
  }

  factorize_primes(n, factors);

  return factors;
}

template<typename FloatTypeX, typename FloatTypeY>
pod_vector<int64_t> factorize_square(int64_t n) {
  // Factorize using the kernel lengths we have available
  pod_vector<int64_t> factors;
  factors.reserve(internal::max_factors);
  pod_vector<int> k_facts = get_kernel_ns<FloatTypeX, FloatTypeY>();

  // Make n as square/cube/hypercube as possible...

  pod_vector<int64_t> k_facts_useful;
  for (auto factor : k_facts) {
    if (n % factor == 0) {
      k_facts_useful.push_back(factor);
    }
  }

  while (!k_facts_useful.empty()) {
    auto base = k_facts_useful[0]; // This is the highest factor we can use at
                                   // this stage

    // log_base(n) gives the minimum number of factors required, base being the
    // highest possible base case
    auto lg = log_base(n, base);

    // Find the highest base factor we should use to make the problem as
    // hypercube as possible First find the root of the base factor
    auto top_factor = (int64_t)std::floor(std::pow(n, 1. / (double)lg));
    // Now search the list of base factors to find the next factor up from the
    // root
    bool unset = true;
    for (size_t i = 0; i < k_facts_useful.size() - 1; i++) {
      if (k_facts_useful[i] >= top_factor &&
          k_facts_useful[i + 1] < top_factor) {
        top_factor = k_facts_useful[i];
        unset = false;
        break;
      }
    }
    if (unset) {
      top_factor = k_facts_useful.back();
    }

    factors.push_back(top_factor);

    n /= top_factor;

    // Find the useful factors for the next iteration
    k_facts_useful.clear();
    for (auto factor : k_facts) {
      if (n % factor == 0) {
        k_facts_useful.push_back(factor);
      }
    }
  }

  factorize_primes(n, factors);

  return factors;
}

template<typename FloatTypeX, typename FloatTypeY>
pod_vector<int64_t> factorize(int64_t n) {
  auto f_dsc = factorize_descending<FloatTypeX, FloatTypeY>(n);
  auto f_sq = factorize_square<FloatTypeX, FloatTypeY>(n);

  return f_dsc.size() < f_sq.size() ? f_dsc : f_sq;
}

inline bool is_base_n(int64_t n, const pod_vector<int> &kernel_factors) {
  return std::find(kernel_factors.cbegin(), kernel_factors.cend(), n) !=
         kernel_factors.cend();
}

template<typename FloatTypeX, typename FloatTypeY>
inline bool is_base_case(int64_t n, const pod_vector<int> &kernel_factors) {
  auto factors = factorize<FloatTypeX, FloatTypeY>(n);
  return factors.size() == 1 && is_base_n(n, kernel_factors);
}

} // end namespace plfft
