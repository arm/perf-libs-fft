/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "compositor.hpp"
#include "exec_compositor.hpp"
#include "factorize.hpp"
#include "kernel_data.hpp"
#include "make_level_data.hpp"
#include "plfft_timer.hpp"

namespace plfft {

#ifndef NO_LIBCPP
template<typename Tx, typename Ty>
std::string composition_to_string(const composition<Tx, Ty> &c) {
  if (c.nlevels == 1) {
    return c.levels.front()->level_to_string();
  }
  std::ostringstream sstm;
  sstm << "(cooley-tukey ";
  sstm << c.n << ' ';
  for (const auto &level : c.levels) {
    sstm << level->level_to_string() << " ";
  }
  sstm << ")";
  return std::move(sstm).str();
}

#define COMPOSITION_TO_STRING(Tx, Ty)                                          \
  template std::string composition_to_string<Tx, Ty>(                          \
      const composition<Tx, Ty> &);

COMPOSITION_TO_STRING(half, std::complex<half>)
COMPOSITION_TO_STRING(std::complex<half>, half)
COMPOSITION_TO_STRING(std::complex<half>, std::complex<half>)
COMPOSITION_TO_STRING(float, std::complex<float>)
COMPOSITION_TO_STRING(std::complex<float>, float)
COMPOSITION_TO_STRING(std::complex<float>, std::complex<float>)
COMPOSITION_TO_STRING(double, std::complex<double>)
COMPOSITION_TO_STRING(std::complex<double>, double)
COMPOSITION_TO_STRING(std::complex<double>, std::complex<double>)
#if PLFFT_ENABLE_FIXED_POINT
COMPOSITION_TO_STRING(int8_t, std::complex<int8_t>)
COMPOSITION_TO_STRING(std::complex<int8_t>, int8_t)
COMPOSITION_TO_STRING(std::complex<int8_t>, std::complex<int8_t>)
COMPOSITION_TO_STRING(int16_t, std::complex<int16_t>)
COMPOSITION_TO_STRING(std::complex<int16_t>, int16_t)
COMPOSITION_TO_STRING(std::complex<int16_t>, std::complex<int16_t>)
#endif // PLFFT_ENABLE_FIXED_POINT

#undef COMPOSITION_TO_STRING
#endif // NO_LIBCPP

template<typename Tx, typename Ty>
static std::pair<bool, std::optional<composition<Tx, Ty>>>
composite_init_from_factors(int64_t n, int64_t howmany, int64_t istride,
                            int64_t idist, int64_t ostride, int64_t odist,
                            plfft_direction_t dir,
                            const pod_vector<int64_t> &factors,
                            double target_secs_total, double margin,
                            bool allow_raders, bool allow_bluestein,
                            bool want_sme) {
  // Build a composition from a provided set of factors.
  // We also return a boolean that says whether we should consider re-running
  // this composition with Bluestein rather than Rader's algorithm for prime
  // factor cases.
  composition<Tx, Ty> comp{n, dir, 0, {}, {}};

  // if we end up using Rader's, we should consider also using Bluestein
  // if we have time to do so.
  bool could_try_bluestein = false;

  // Keep track of the running product of factors as we iterate from left
  // to right, we will need this as it forms n2 for non-base cases.
  int64_t running_product = is_dit_v<Tx, Ty> ? 1 : n / factors[0];

  // We build the composition as a left-associative product of factors,
  // i.e. ((n1*n2)*n3)*... where n1 >= n2 >= n3
  for (unsigned fi = 0; fi < factors.size(); ++fi) {
    auto n1 = factors[fi];
    int64_t n2;
    bool want_twiddles;
    if constexpr (is_dit_v<Tx, Ty>) {
      if (fi == 0) {
        n2 = factors.size() > 1 ? factors[1] : 1;
      } else {
        n2 = running_product;
      }
      running_product *= n1;
      want_twiddles = fi > 0;
    } else {
      if (fi == factors.size() - 1) {
        n2 = factors.size() > 1 ? factors[0] : 1;
      } else {
        n2 = running_product;
      }
      running_product /= (fi + 1 < factors.size()) ? factors[fi + 1] : 1;
      want_twiddles = fi < factors.size() - 1;
    }
    auto hm_lev = n / (n1 * n2);
    std::optional<level_data_info> maybe_lev_info;

    comp.levels.emplace_back();
    // For single level, it is always a non-twiddled level.
    if (factors.size() == 1) {
      maybe_lev_info = make_level_data<level_type::SINGLE>(
          &comp.levels.back(), n, n1, n2, hm_lev, howmany, istride, idist,
          ostride, odist, dir, target_secs_total, margin, allow_raders,
          allow_bluestein, want_twiddles, want_sme);
    }
    // First level is non-twiddled for DIT.
    else if (fi == 0) {
      maybe_lev_info = make_level_data<level_type::INPUT>(
          &comp.levels.back(), n, n1, n2, hm_lev, howmany, istride, idist,
          ostride, odist, dir, target_secs_total, margin, allow_raders,
          allow_bluestein, want_twiddles, want_sme);
    }
    // Last level is non-twiddled for DIF.
    else if (fi == factors.size() - 1) {
      maybe_lev_info = make_level_data<level_type::OUTPUT>(
          &comp.levels.back(), n, n1, n2, hm_lev, howmany, istride, idist,
          ostride, odist, dir, target_secs_total, margin, allow_raders,
          allow_bluestein, want_twiddles, want_sme);
    } else {
      maybe_lev_info = make_level_data<level_type::INTERNAL>(
          &comp.levels.back(), n, n1, n2, hm_lev, howmany, istride, idist,
          ostride, odist, dir, target_secs_total, margin, allow_raders,
          allow_bluestein, want_twiddles, want_sme);
    }
    if (!maybe_lev_info) {
      return {false, std::nullopt};
    }
    could_try_bluestein |= maybe_lev_info->used_rader;
    comp.nlevels++;
  }
  return {could_try_bluestein, std::move(comp)};
}

static constexpr int64_t factorial(int64_t n) {
  return n <= 1 ? 1 : n * factorial(n - 1);
}

static inline std::pair<int64_t, int64_t>
get_audition_buffer_strides(const int64_t istride, const int64_t ostride) {
  // Use buffer strides == 2 instead of original strides when original
  // strides are non-unit to save memory. This already ensures that the
  // same kernel types are use for auditioning.
  return {istride == 1 ? 1 : 2, ostride == 1 ? 1 : 2};
}

template<typename Tx, typename Ty>
static void composite_init_set_estimate(
    const pod_vector<int64_t> &factors, composition<Tx, Ty> &comp, int64_t n,
    const Tx *in, Ty *out, const int64_t istride, const int64_t ostride,
    double target_secs, statistics::normal_distribution candidate_dist,
    double margin) {

  auto &dist = comp.estimate;

  auto [buf_is, buf_os] = get_audition_buffer_strides(istride, ostride);

  // Execute once to warm up the cache for small problems
  if (target_secs > 0.0) {
    execute(comp, 1, in, out, buf_is, buf_os, 0, 0);
  }

  // Candidate_dist is the distribution of the current-best candidate,
  // margin is the point at which we give up early if possible
  // (e.g. if we have a 5% chance or less of being better than the
  // current candidate)
  double elapsed_secs = 0;
  while (elapsed_secs + dist.mean < target_secs) {
    auto t = timer_start();
    execute(comp, 1, in, out, buf_is, buf_os, 0, 0);
    double diff_secs = timer_end(std::move(t));
    elapsed_secs += diff_secs;
    dist = statistics::sample_normal_incremental(dist, diff_secs);
    auto prob = statistics::welch_t_test(candidate_dist, dist).v1_p;
    if (prob < margin || prob > (1 - margin)) {
      break;
    }
  }
}

template<typename Tx, typename Ty>
static bool audition_perms(pod_vector<int64_t> factors, int64_t n, const Tx *in,
                           Ty *out, int64_t howmany, int64_t istride,
                           int64_t idist, int64_t ostride, int64_t odist,
                           plfft_direction_t dir, double target_secs_total,
                           double margin, composition<Tx, Ty> &comp,
                           bool allow_convolutions, bool first, bool want_sme) {
  // TODO this is good enough for now, but when we have more things to choose
  // from then it would be much more useful to actually prioritize the choices
  // we think will actually benefit us rather than testing all equally.
  int64_t nperms = target_secs_total == 0.0 ? 1 : factorial(factors.size());
  double target_secs_each = target_secs_total / (double)nperms;

  auto [buf_is, buf_os] = get_audition_buffer_strides(istride, ostride);

  auto t = timer_start();
  for (int64_t i = 0; i < nperms; ++i, first = false) {
    // Try constructing a composition candidate with howmany = 1 and buffer
    // strides. If we allow convolutions then allow both Rader's and Bluestein
    // algorithms (hence the double "allow_convolutions").
    auto [could_try_bluestein, comp_candidate] =
        composite_init_from_factors<Tx, Ty>(
            n, 1, buf_is, 0, buf_os, 0, dir, factors, target_secs_total, margin,
            allow_convolutions, allow_convolutions, want_sme);
    if (!comp_candidate) {
      return false;
    }
    composite_init_set_estimate(factors, *comp_candidate, n, in, out, istride,
                                ostride, target_secs_each, comp.estimate,
                                margin);
    if (first || comp_candidate->estimate.mean < comp.estimate.mean) {
      // Create new composition with input howmany/strides/dists.
      comp = *composite_init_from_factors<Tx, Ty>(
                  n, howmany, istride, idist, ostride, odist, dir, factors,
                  target_secs_total, margin, allow_convolutions,
                  allow_convolutions, want_sme)
                  .second;
      comp.estimate = comp_candidate->estimate;
    }
    double diff_secs = timer_end(t);
    if (diff_secs >= target_secs_total) {
      // we took too long, make do with what we've found so far
      break;
    }

    // if we tried a composition using Rader's algorithm, a good candidate for
    // improving performance is simply to try Bluestein instead. Whether this
    // is possible or not is given to us by the previous call to
    // composite_init_from_factors.
    if (could_try_bluestein) {
      comp_candidate = composite_init_from_factors<Tx, Ty>(
                           n, 1, buf_is, 0, buf_os, 0, dir, factors,
                           target_secs_total, margin, false, true, want_sme)
                           .second;
      assert(comp_candidate);
      composite_init_set_estimate(factors, *comp_candidate, n, in, out, istride,
                                  ostride, target_secs_each, comp.estimate,
                                  margin);
      if (comp_candidate->estimate.mean < comp.estimate.mean) {
        comp = *composite_init_from_factors<Tx, Ty>(
                    n, howmany, istride, idist, ostride, odist, dir, factors,
                    target_secs_total, margin, allow_convolutions,
                    allow_convolutions, want_sme)
                    .second;
      }
      diff_secs = timer_end(t);
      if (diff_secs >= target_secs_total) {
        // we took too long, make do with what we've found so far
        break;
      }
    }
    std::next_permutation(factors.begin(), factors.end());
  }

  return true;
}

template<typename Tx, typename Ty>
std::pair<bool, composition<Tx, Ty>>
composite_init(int64_t n, int64_t howmany, int64_t istride, int64_t idist,
               int64_t ostride, int64_t odist, plfft_direction_t dir,
               double target_secs_total, double margin, bool allow_convolutions,
               bool want_sme) {
  composition<Tx, Ty> comp{n, dir, {}};

  if (n < 2) {
    return {true, std::move(comp)};
  }

  if (get_kernel_ns<Tx, Ty>().empty()) {
    // if we can't factorise anything (e.g. if we're trying to use
    // half precision routines when we don't have any appropriate
    // kernels) don't use central!
    return {false, std::move(comp)};
  }

  // We do not pass in the in/out pointers from the caller since we do not know
  // the istride/ostride of the data.
  Tx *in = nullptr;
  Ty *out = nullptr;
  pod_vector<Tx> tmp_in;
  pod_vector<Ty> tmp_out;
  auto [buf_is, buf_os] = get_audition_buffer_strides(istride, ostride);
  tmp_in.resize(n * buf_is);
  tmp_out.resize(n * buf_os);
  in = tmp_in.data();
  out = tmp_out.data();

  // Audition permutations of both factorizations
  // Using a square factorization is the default
  const auto factors_sqr = factorize_square<Tx, Ty>(n);
  auto success1 = audition_perms(
      factors_sqr, n, in, out, howmany, istride, idist, ostride, odist, dir,
      target_secs_total, margin, comp, allow_convolutions, true, want_sme);
  // If above failed or target_secs_total is positive then try descending
  // factorization as well
  bool success2 = true;
  if (!success1 || target_secs_total > 0.0) {
    const auto factors_dsc = factorize_descending<Tx, Ty>(n);
    success2 = audition_perms(factors_dsc, n, in, out, howmany, istride, idist,
                              ostride, odist, dir, target_secs_total, margin,
                              comp, allow_convolutions, false, want_sme);
  }
  if (!success2) {
    return {false, std::move(comp)};
  }

  return {true, std::move(comp)};
}

template<typename Tx, typename Ty>
algo_flops composition<Tx, Ty>::flops() const {
  algo_flops ret;
  for (auto &&lev : levels) {
    ret += lev->flops();
  }
  return ret;
}

template struct composition<half, std::complex<half>>;
template struct composition<std::complex<half>, half>;
template struct composition<std::complex<half>, std::complex<half>>;
template struct composition<float, std::complex<float>>;
template struct composition<std::complex<float>, float>;
template struct composition<std::complex<float>, std::complex<float>>;
template struct composition<double, std::complex<double>>;
template struct composition<std::complex<double>, double>;
template struct composition<std::complex<double>, std::complex<double>>;
#if PLFFT_ENABLE_FIXED_POINT
template struct composition<int8_t, std::complex<int8_t>>;
template struct composition<std::complex<int8_t>, int8_t>;
template struct composition<std::complex<int8_t>, std::complex<int8_t>>;
template struct composition<int16_t, std::complex<int16_t>>;
template struct composition<std::complex<int16_t>, int16_t>;
template struct composition<std::complex<int16_t>, std::complex<int16_t>>;
#endif // PLFFT_ENABLE_FIXED_POINT

#define COMPOSITE_INIT(Tx, Ty)                                                 \
  template std::pair<bool, composition<Tx, Ty>> composite_init(                \
      int64_t n, int64_t howmany, int64_t istride, int64_t idist,              \
      int64_t ostride, int64_t odist, plfft_direction_t dir,                   \
      double target_secs_total, double margin, bool allow_convolutions,        \
      bool want_sme);

COMPOSITE_INIT(half, std::complex<half>)
COMPOSITE_INIT(std::complex<half>, half)
COMPOSITE_INIT(std::complex<half>, std::complex<half>)
COMPOSITE_INIT(float, std::complex<float>)
COMPOSITE_INIT(std::complex<float>, float)
COMPOSITE_INIT(std::complex<float>, std::complex<float>)
COMPOSITE_INIT(double, std::complex<double>)
COMPOSITE_INIT(std::complex<double>, double)
COMPOSITE_INIT(std::complex<double>, std::complex<double>)
#if PLFFT_ENABLE_FIXED_POINT
COMPOSITE_INIT(int8_t, std::complex<int8_t>)
COMPOSITE_INIT(std::complex<int8_t>, int8_t)
COMPOSITE_INIT(std::complex<int8_t>, std::complex<int8_t>)
COMPOSITE_INIT(int16_t, std::complex<int16_t>)
COMPOSITE_INIT(std::complex<int16_t>, int16_t)
COMPOSITE_INIT(std::complex<int16_t>, std::complex<int16_t>)
#endif // PLFFT_ENABLE_FIXED_POINT

#undef COMPOSITE_INIT

} // end namespace plfft
