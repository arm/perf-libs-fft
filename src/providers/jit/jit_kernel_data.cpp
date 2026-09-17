/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "kernel_data.hpp"

#include "algo.hpp"
#include "cpu_features.hpp"
#include "kernel_cache.hpp"
#include "kernel_printer.hpp"
#include "kernel_provider_capabilities.hpp"
#include "plfft.h"
#include "plfft_assert.hpp"
#include "plfft_kernels.hpp"
#include "plfft_lazy.hpp"
#include "print_algo.hpp"
#include "runtime_precision.hpp"
#include "vector_size.hpp"
#include "wfta_nfact.hpp"
#include "winograd_reorder.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace plfft {

bool kernel_provider_emulates_fp16() {
  return true;
}

static bool want_vla() {
  static bool ret = [] {
    const char *env_var = getenv("PLFFT_WFTA_VLA");
    return env_var && strcmp(env_var, "1") == 0;
  }();
  return ret;
}

static bool want_map_file() {
  static bool ret = [] {
    const char *env_var = getenv("PLFFT_WFTA_PERF");
    return env_var && strcmp("1", env_var) == 0;
  }();
  return ret;
}

static bool want_check_kernel_hash() {
  static bool ret = [] {
    const char *env_var = getenv("PLFFT_WFTA_CHECK_HASH");
    return env_var && strcmp("1", env_var) == 0;
  }();
  return ret;
}

static bool want_kernel_src() {
  static bool ret = [] {
    const char *env_var = getenv("PLFFT_WFTA_KERNEL_SRC");
    return env_var && strcmp("1", env_var) == 0;
  }();
  return ret;
}

template<typename Tx, typename Tw>
static constexpr bool can_use_structure_load(int64_t n) {
  using Rx = remove_complex_t<Tx>;
  using Rw = remove_complex_t<Tw>;
  if constexpr (std::is_integral_v<Rx> || !std::is_same_v<Rx, Rw> ||
                std::is_same_v<Rx, double>) {
    return false;
  }
  return n == 2 || n == 3 || n == 4 ||
         (std::is_same_v<Rx, half> && (n == 6 || n == 8));
}

template<typename Ty, typename Tw>
static constexpr bool can_use_structure_store(int64_t n) {
  using Ry = remove_complex_t<Ty>;
  using Rw = remove_complex_t<Tw>;
  if constexpr (!is_complex_v<Ty> || std::is_integral_v<Ry> ||
                !std::is_same_v<Ry, Rw> || std::is_same_v<Ry, double>) {
    return false;
  }
  return n == 2 || n == 3 || n == 4 ||
         (std::is_same_v<Ry, half> && (n == 6 || n == 8));
}

template<typename Tx, typename Tw>
static bool
can_use_sme_transposing_load(const lazy<wfta::kernel_algo_data> &data) {
  using Rx = remove_complex_t<Tx>;
  using Rw = remove_complex_t<Tw>;
  if constexpr (std::is_integral_v<Rx> || !std::is_same_v<Rx, Rw> ||
                std::is_same_v<Rx, double>) {
    return false;
  }

  // This function may apply to several kernels at once - for r2c we will still
  // get a kernel with complex input, so have to make the decision about whether
  // to use tile transpose based on the complex element size
  constexpr int worst_case_input_elem_bytes = sizeof(std::complex<Rx>);
  constexpr int tile_dim = 64 / worst_case_input_elem_bytes;
  constexpr int max_tile_count = sizeof(Rx);
  return wfta::get_in_size_elems(data->iop) <= max_tile_count * tile_dim;
}

static wfta::options_t get_options(bool has_sve, bool want_sme) {
  std::optional<int> vector_length_bytes;
  if (want_sme) {
    vector_length_bytes = 64;
  } else if (!has_sve) {
    vector_length_bytes = 16;
  } else if (!want_vla()) {
    vector_length_bytes = vector_size_bytes(want_sme);
  }
  auto features = get_cpu_features();
  const wfta::target_t target = {
      // Name does not match AOT in the case of kernels which use ASIMDHP
      .name = want_sme ? "sme" : (has_sve ? "sve" : "neon"),
      .has_fcma = features.fcma,
      .has_sve = has_sve,
      .has_sme = want_sme,
      .known_vector_length_bytes = vector_length_bytes};
  return wfta::options_t{.target = target,
                         .want_map_file = want_map_file(),
                         .want_check_kernel_hash = want_check_kernel_hash(),
                         .want_kernel_src = want_kernel_src(),
                         .want_sme = want_sme};
}

static std::string factors_to_string(const std::vector<int> &factors,
                                     const char *sep) {
  std::ostringstream sstm;
  for (unsigned i = 0; i < factors.size(); ++i) {
    if (i) {
      sstm << sep;
    }
    sstm << factors[i];
  }
  return std::move(sstm).str();
}

static inline constexpr int factorial(int n) {
  return n <= 1 ? 1 : n * factorial(n - 1);
}

static inline bool is_supported_factor(int i) {
  static std::set<int> supported{1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 16, 17, 19};
  return supported.find(i) != supported.end();
}

static inline bool is_supported_product(int i) {
  // Some products are hard-coded in a way that doesn't work with the
  // WFTA algorithm. These are listed here.
  static std::set<int> supported{25, 32};
  return supported.find(i) != supported.end();
}

static std::vector<int> get_factorisation(int prod) {
  ASSERT(prod > 1);
  std::vector<int> ret;
  if (is_supported_product(prod)) {
    return {prod};
  }
  for (int i = prod; prod > 1 && i > 1; --i) {
    if (prod % i == 0 && is_supported_factor(i)) {
      ret.push_back(i);
      prod /= i;
    }
  }
  if (prod != 1) {
    return {};
  }
  return ret;
}

template<typename Tw>
static inline int get_ab_twid_interleave_factor(const wfta::options_t &opts) {
  if (!opts.target.has_sve) {
    return 1;
  }
  if (opts.target.known_vector_length_bytes) {
    return *opts.target.known_vector_length_bytes / sizeof(Tw);
  }
  // note we cannot use vector_size_elems, since as time of writing this will
  // return 0 if the library is not configured to use SVE.
  return vector_size_bytes(opts.want_sme) / sizeof(Tw);
}

wfta::kernel_registry_map kernel_cache::registry = {};
std::vector<std::tuple<void *, size_t>> kernel_cache::allocated_mem = {};

template<typename Tx, typename Ty, typename Tw, typename = void>
struct kernel_data_generator;

template<typename Tx, typename Ty, typename Tw>
struct kernel_data_generator<Tx, Ty, Tw, std::enable_if_t<is_c2c_v<Tx, Ty>>> {
  static kernel_data<Tx, Ty>
  get_from_factors(kernel_data<Tx, Ty> ret, const wfta::options_t &opts,
                   const wfta::known_layout_t &known_layout, int64_t n,
                   plfft_direction_t dir, lazy<wfta::kernel_algo_data> &data,
                   const std::string &mid, order_kind order) {
    wfta::kernel_printer<Tx, Ty, Tw> kp{
        kernel_cache::registry, data, n, mid, dir, opts, known_layout};

    switch (order) {
    case order_kind::ORDER_NA:
      kp.print_algo(&ret.ab_n, wfta::twiddleness::none, order,
                    {out_mods::om_none, in_mods::im_none});
      break;
    case order_kind::ORDER_AB:
      kp.print_algo(&ret.ab_t_dit, wfta::twiddleness::dit, order,
                    {out_mods::om_none, in_mods::im_none});
      break;
    case order_kind::ORDER_AC:
      kp.print_algo(&ret.ac_t_dit, wfta::twiddleness::dit, order,
                    {out_mods::om_none, in_mods::im_none});
      break;
    }

    kp.emit();
    return ret;
  }
};

template<typename Tx, typename Ty, typename Tw>
struct kernel_data_generator<Tx, Ty, Tw, std::enable_if_t<is_r2c_v<Tx, Ty>>> {
  static kernel_data<Tx, Ty>
  get_from_factors(kernel_data<Tx, Ty> ret, const wfta::options_t &opts,
                   const wfta::known_layout_t &known_layout, int64_t n,
                   plfft_direction_t, lazy<wfta::kernel_algo_data> &data,
                   const std::string &mid, order_kind order) {
    auto dir = PLFFT_FORWARD;
    wfta::kernel_printer<Tx, Ty, Tw> kp{
        kernel_cache::registry, data, n, mid, dir, opts, known_layout};

    switch (order) {
    case order_kind::ORDER_NA:
      kp.print_algo(&ret.ab_n, wfta::twiddleness::none, order,
                    {out_mods::halfhi, in_mods::im_real});
      kp.print_algo(&ret.ab_nfoh, wfta::twiddleness::none, order,
                    {out_mods::halfhi, in_mods::im_none});
      break;
    case order_kind::ORDER_AB:
      kp.print_algo(&ret.ab_tfj_dit, wfta::twiddleness::dit, order,
                    {out_mods::conj_reverse, in_mods::im_none});
      kp.print_algo(&ret.ab_tfol_dit, wfta::twiddleness::dit, order,
                    {out_mods::halflo, in_mods::im_none});
      break;
    case order_kind::ORDER_AC:
      kp.print_algo(&ret.ac_tfj_dit, wfta::twiddleness::dit, order,
                    {out_mods::conj_reverse, in_mods::im_none});
      kp.print_algo(&ret.ac_tfol_dit, wfta::twiddleness::dit, order,
                    {out_mods::halflo, in_mods::im_none});
      break;
    }

    kp.emit();
    return ret;
  }
};

template<typename Tx, typename Ty, typename Tw>
struct kernel_data_generator<Tx, Ty, Tw, std::enable_if_t<is_c2r_v<Tx, Ty>>> {
  static kernel_data<Tx, Ty>
  get_from_factors(kernel_data<Tx, Ty> ret, const wfta::options_t &opts,
                   const wfta::known_layout_t &known_layout, int64_t n,
                   plfft_direction_t, lazy<wfta::kernel_algo_data> &data,
                   const std::string &mid, order_kind order) {
    auto dir = PLFFT_BACKWARD;
    wfta::kernel_printer<Tx, Ty, Tw> kp{
        kernel_cache::registry, data, n, mid, dir, opts, known_layout};

    switch (order) {
    case order_kind::ORDER_NA:
      kp.print_algo(&ret.ab_n, wfta::twiddleness::none, order,
                    {out_mods::om_real, in_mods::im_none});
      kp.print_algo(&ret.ab_nbih, wfta::twiddleness::none, order,
                    {out_mods::om_none, in_mods::im_halfhi});
      break;
    case order_kind::ORDER_AB:
      if (n % 2 == 0) {
        kp.print_algo(&ret.ab_tbil_dif, wfta::twiddleness::dif, order,
                      {out_mods::om_none, in_mods::im_halflo});
      } else {
        kp.print_algo(&ret.ab_tbih_dif, wfta::twiddleness::dif, order,
                      {out_mods::om_none, in_mods::im_halfhi});
      }
      break;
    case order_kind::ORDER_AC:
      if (n % 2 == 0) {
        kp.print_algo(&ret.ac_tbil_dif, wfta::twiddleness::dif, order,
                      {out_mods::om_none, in_mods::im_halflo});
      } else {
        kp.print_algo(&ret.ac_tbih_dif, wfta::twiddleness::dif, order,
                      {out_mods::om_none, in_mods::im_halfhi});
      }
      break;
    }

    kp.emit();
    return ret;
  }
};

static bool is_coprime(int n1, int n2) {
  while (n2) {
    auto t = n1 % n2;
    n1 = n2;
    n2 = t;
  }
  return n1 == 1;
}

template<typename Tx, typename Ty, typename Tw>
static std::pair<kernel_data<Tx, Ty>, wfta::options_t>
init_kernel_data_and_opts(order_kind order, bool want_sve, bool want_sme) {
  kernel_data<Tx, Ty> kd;
  kd.twid_layout.precision =
      runtime_precision_from_real_type<remove_complex_t<Tw>>();

  const auto opts = get_options(want_sve, want_sme);
  kd.twid_layout.want_premul = !want_sve && !opts.target.has_fcma;

  if (order == order_kind::ORDER_AB && opts.target.has_sve) {
    // AB kernels with SVE-style twiddle layout can load W[j:j+VL]
    // consecutively, avoiding gather loads.
    kd.twid_layout.interleave_factor = get_ab_twid_interleave_factor<Tw>(opts);
  } else {
    kd.twid_layout.interleave_factor = 1;
  }
  return {kd, opts};
}

template<typename Tx, typename Ty, typename Tw>
static kernel_data<Tx, Ty> get_kernel_data_wtype(
    int64_t n, const std::vector<int> &factors, std::optional<int64_t> howmany,
    std::optional<int64_t> istride, std::optional<int64_t> ostride,
    std::optional<int64_t> idist, std::optional<int64_t> odist,
    plfft_direction_t dir, int strategy, order_kind order, bool want_sve,
    bool want_sme) {
  constexpr bool is_fixed_point = std::is_integral_v<remove_complex_t<Tw>>;
  auto data = make_lazy([&] {
    auto nfact = wfta::wfta_nfact(factors, is_fixed_point);
    auto in_perm = wfta::get_in_perm(factors);
    auto out_perm = wfta::get_out_perm(factors);
    return wfta::kernel_algo_data{std::move(nfact.algo), std::move(in_perm),
                                  std::move(out_perm), std::move(nfact.iop)};
  });

  std::string mid = factors_to_string(factors, "_");

  using Kgen = kernel_data_generator<Tx, Ty, Tw>;

  wfta::known_layout_t known_layout;
  bool opt_sve = want_sve || want_sme;
  bool opt_sme = want_sme;

  using Rw = remove_complex_t<Tw>;
  constexpr int neon_unroll = 8 / sizeof(Rw);
  const bool can_use_neon_tu = !want_sve && !want_sme && howmany &&
                               *howmany % neon_unroll == 0 &&
                               can_use_structure_load<Tx, Tw>(n);
  const bool can_use_neon_ut = !want_sve && !want_sme && howmany &&
                               *howmany % neon_unroll == 0 &&
                               can_use_structure_store<Ty, Tw>(n);
  const bool can_use_sve_tu = want_sve && can_use_structure_load<Tx, Tw>(n);
  const bool can_use_sme_tu =
      want_sme && can_use_sme_transposing_load<Tx, Tw>(data);
  const bool can_use_sve_ut = want_sve && can_use_structure_store<Ty, Tw>(n);
  const bool can_use_sme_ut = want_sme && can_use_structure_store<Ty, Tw>(n);
  const bool can_use_tu = istride == 1 && idist == n && odist == 1 &&
                          (can_use_neon_tu || can_use_sve_tu || can_use_sme_tu);
  const bool can_use_ut = idist == 1 && ostride == 1 && odist == n &&
                          (can_use_neon_ut || can_use_sve_ut || can_use_sme_ut);
  const bool can_use_tt = istride == 1 && idist == n && ostride == 1 &&
                          odist == n &&
                          ((can_use_neon_tu && can_use_neon_ut) ||
                           (can_use_sve_tu && can_use_sve_ut) ||
                           (can_use_sme_tu && can_use_sme_ut));

  if (howmany == 1) {
    // Always use Neon for uun
    known_layout.howmany = 1;
    known_layout.idist = 1;
    known_layout.odist = 1;
    opt_sve = false;
    opt_sme = false;
  } else if (idist == 1 && odist == 1) {
    // uu
    known_layout.idist = 1;
    known_layout.odist = 1;
  } else if (can_use_tt) {
    // tt
    known_layout.istride = 1;
    known_layout.idist = n;
    known_layout.ostride = 1;
    known_layout.odist = n;
  } else if (can_use_tu) {
    // tu
    known_layout.istride = 1;
    known_layout.idist = n;
    known_layout.odist = 1;
  } else if (can_use_ut) {
    // ut
    known_layout.idist = 1;
    known_layout.ostride = 1;
    known_layout.odist = n;
  } else {
    // gather/scatter layouts - force SME off
    opt_sve = want_sve;
    opt_sme = false;
    if (idist == 1) {
      known_layout.idist = 1;
    } else if (odist == 1) {
      known_layout.odist = 1;
    }
  }

  const auto [ret, opts] =
      init_kernel_data_and_opts<Tx, Ty, Tw>(order, opt_sve, opt_sme);
  return Kgen::get_from_factors(ret, opts, known_layout, n, dir, data, mid,
                                order);
}

static bool are_all_coprime(const std::vector<int> &factors) {
  ASSERT(!factors.empty());
  for (int f1 : factors) {
    for (int f2 : factors) {
      if (f1 != f2 && !is_coprime(f1, f2)) {
        return false;
      }
    }
  }
  return true;
}

/// Returns the fft kernels for a particular n and plfft_direction_t.
template<typename Tx, typename Ty>
std::optional<kernel_data<Tx, Ty>>
get_kernel_data(int64_t n, std::optional<int64_t> howmany,
                std::optional<int64_t> istride, std::optional<int64_t> ostride,
                std::optional<int64_t> idist, std::optional<int64_t> odist,
                plfft_direction_t dir, int strategy, order_kind order,
                bool want_sme) {
  static_assert(std::is_same_v<remove_complex_t<Tx>, remove_complex_t<Ty>>,
                "Tx and Ty must have the same non-complex type");

  const auto &provider_ns = get_kernel_ns<Tx, Ty>();
  if (std::find(provider_ns.cbegin(), provider_ns.cend(), n) ==
      provider_ns.cend()) {
    return std::nullopt;
  }

  auto factors = get_factorisation((int)n);
  if (!are_all_coprime(factors)) {
    return {};
  }
  auto nstrats = factorial((int)factors.size());
  if (strategy >= nstrats) {
    return {};
  }
  for (int i = 0; i < strategy; ++i) {
    std::next_permutation(factors.begin(), factors.end());
  }

  auto features = get_cpu_features();
  auto want_sve = features.sve && get_sve<Tx, Ty>();
  if (want_sve || want_sme) {
    // use SVE if available (in which case we must have have half-precision).
    // Ignore features if want_sme - SME kernel has been explicitly requested so
    // it's not up to us to make sure that SME is actually present.
    ASSERT(features.asimdhp || want_sme);
    // Generate half-precision gather / scatter SVE kernels only if:
    //
    //   INT32_MIN <= (VL/32 - 1) * {i,o}dist <= INT32_MAX
    //
    // otherwise generate Neon kernels.
    //
    // This ensures idist and odist can safely be used as increment values
    // in the 32-bit SVE index instructions used by the half-precision SVE
    // kernels without causing overflow or underflow.
    //
    // Single and double precision SVE kernels use 64-bit index instructions
    // so are not subject to this restriction.
    using Tw = add_complex_t<Tx>;
    const auto cntw = vector_size_bytes(want_sme) / sizeof(complex_half);
    constexpr auto limit = std::numeric_limits<int32_t>::max();
    if constexpr (std::is_same_v<remove_complex_t<Tx>, half>) {
      if (idist && limit < (cntw - 1) * std::abs(*idist)) {
        return get_kernel_data_wtype<Tx, Ty, Tw>(n, factors, howmany, istride,
                                                 ostride, idist, odist, dir,
                                                 strategy, order, false, false);
      }
    }
    if constexpr (std::is_same_v<remove_complex_t<Ty>, half>) {
      if (odist && limit < (cntw - 1) * std::abs(*odist)) {
        return get_kernel_data_wtype<Tx, Ty, Tw>(n, factors, howmany, istride,
                                                 ostride, idist, odist, dir,
                                                 strategy, order, false, false);
      }
    }
  }
  if constexpr (std::is_same_v<remove_complex_t<Tx>, half>) {
    if (!features.asimdhp) {
      // if we do not have native half-precision, emulate with single-precision.
      using Tw = std::complex<float>;
      return get_kernel_data_wtype<Tx, Ty, Tw>(n, factors, howmany, istride,
                                               ostride, idist, odist, dir,
                                               strategy, order, false, false);
    }
  }
  using Tw = add_complex_t<Tx>;
  return get_kernel_data_wtype<Tx, Ty, Tw>(n, factors, howmany, istride,
                                           ostride, idist, odist, dir, strategy,
                                           order, want_sve, want_sme);
}

#define GET_KERNEL_DATA(Tx, Ty)                                                \
  template std::optional<kernel_data<Tx, Ty>> get_kernel_data(                 \
      int64_t n, std::optional<int64_t> howmany,                               \
      std::optional<int64_t> istride, std::optional<int64_t> ostride,          \
      std::optional<int64_t> idist, std::optional<int64_t> odist,              \
      plfft_direction_t dir, int strategy, order_kind order, bool want_sme);

GET_KERNEL_DATA(half, std::complex<half>)
GET_KERNEL_DATA(std::complex<half>, half)
GET_KERNEL_DATA(std::complex<half>, std::complex<half>)
GET_KERNEL_DATA(float, std::complex<float>)
GET_KERNEL_DATA(std::complex<float>, float)
GET_KERNEL_DATA(std::complex<float>, std::complex<float>)
GET_KERNEL_DATA(double, std::complex<double>)
GET_KERNEL_DATA(std::complex<double>, double)
GET_KERNEL_DATA(std::complex<double>, std::complex<double>)
#undef GET_KERNEL_DATA

template<typename Tx, typename Ty>
const pod_vector<int> &get_kernel_ns() {
  // Build a vector of all supported n values in decreasing order.
  // We use a static vector to avoid needing to recompute this.
  static pod_vector<int> ret = [] {
    constexpr int max = 40;
    pod_vector<int> ns;
    ns.reserve(max);
    for (int prod = max; prod > 1; --prod) {
      auto factors = get_factorisation(prod);
      if (factors.empty()) {
        continue;
      }
      if (!are_all_coprime(factors)) {
        continue;
      }
      ns.push_back(prod);
    }
    return ns;
  }();
  return ret;
}

#define GET_KERNEL_NS(Tx, Ty)                                                  \
  template const pod_vector<int> &get_kernel_ns<Tx, Ty>();

GET_KERNEL_NS(half, std::complex<half>)
GET_KERNEL_NS(std::complex<half>, half)
GET_KERNEL_NS(std::complex<half>, std::complex<half>)
GET_KERNEL_NS(float, std::complex<float>)
GET_KERNEL_NS(std::complex<float>, float)
GET_KERNEL_NS(std::complex<float>, std::complex<float>)
GET_KERNEL_NS(double, std::complex<double>)
GET_KERNEL_NS(std::complex<double>, double)
GET_KERNEL_NS(std::complex<double>, std::complex<double>)

#undef GET_KERNEL_NS

} // end namespace plfft
