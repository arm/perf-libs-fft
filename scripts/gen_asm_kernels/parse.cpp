/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "gen_asm.hpp"
#include "plfft_kernels.hpp"
#include "providers/jit/print_algo.hpp"
#include "providers/jit/target.hpp"

#include <complex>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

using namespace plfft;

static bool parse_target(std::string_view name, wfta::target_t *target) {
  // names are enumerated in targets.py
  if (name == "neon") {
    *target = {
        .name = "neon",
        .has_fcma = false,
        .has_sve = false,
        .has_sme = false,
        .known_vector_length_bytes = 16,
    };
  } else if (name == "asimdhp") {
    *target = {
        .name = "asimdhp",
        .has_fcma = false,
        .has_sve = false,
        .has_sme = false,
        .known_vector_length_bytes = 16,
    };
  } else if (name == "sve") {
    *target = {
        .name = "sve",
        .has_fcma = true,
        .has_sve = true,
        .has_sme = false,
        .known_vector_length_bytes = std::nullopt,
    };
  } else if (name == "sme") {
    *target = {
        .name = "sme",
        .has_fcma = true,
        .has_sve = true,
        .has_sme = true,
        .known_vector_length_bytes = 64,
    };
  } else {
    return false;
  }
  return true;
}

enum class type_triple {
  // c2c
  ppp,
  jjj,
  ccc,
  zzz,
  // c2r
  ppr,
  jjh,
  ccs,
  zzd,
  // r2c
  rpp,
  hjj,
  scc,
  dzz,
};

template<unsigned bits>
struct int_of_size;

template<>
struct int_of_size<8> {
  using type = int8_t;
};

template<>
struct int_of_size<16> {
  using type = int16_t;
};

template<>
struct int_of_size<32> {
  using type = int32_t;
};

template<>
struct int_of_size<64> {
  using type = int64_t;
};

template<unsigned bits>
using q = typename int_of_size<bits + 1>::type;

template<unsigned bits>
using cq = std::complex<q<bits>>;

static void usage(const char *argv0) {
  // clang-format off
  fprintf(stderr,
          "Usage: %s [options] <target> <order> <n> <fbits> <types> <twid> <dir> <mod> <dist>\n\n"
          "Positional arguments:\n"
          "  <target>  kernel target        : neon | asimdhp | sve | sme\n"
          "  <order>   iteration order      : - | ab | ac (r2c/c2r require -)\n"
          "  <n>       transform length     : 2 | 3 | ... | 22\n"
          "  <fbits>   fixed-point bits     : - | q7 | q15\n"
          "  <types>   data type triple     : ppp | jjj | ccc | zzz | ppr | jjh | ccs | zzd | rpp | hjj | scc | dzz\n"
          "  <twid>    twiddleness          : n (no twiddle) | dit (input twiddle, DIT) | dif (output twiddle, DIF)\n"
          "  <dir>     transform plfft_direction_t  : f (forward) | b (backward)\n"
          "  <mod>     modifier or -        : - | ol | oh | il | ih | j\n"
          "  <dist>    distribution type    : gu | gs | us | uu | tu | uun\n\n"
          "Options:\n"
          "  -o, --output <path>   Write kernel to the given path.\n"
          "  -h, --help            Show this help and exit.\n",
          argv0);
  // clang-format on
}

template<typename T>
static bool parse_one_of(const std::string &in, T *out,
                         const std::unordered_map<std::string, T> &choices) {
  if (choices.find(in) != choices.end()) {
    *out = choices.at(in);
    return true;
  }
  *out = choices.begin()->second; // to suppress maybe-uninitialized errors
  return false;
}

static bool parse_order(const char *in, order_kind *out) {
  return parse_one_of(in, out,
                      {
                          {"-", order_kind::ORDER_NA},
                          {"ab", order_kind::ORDER_AB},
                          {"ac", order_kind::ORDER_AC},
                      });
}

static bool parse_n(const char *in, int *out) {
  int n = std::atoi(in);
  if (1 < n && n < 23) {
    *out = n;
    return true;
  }
  return false;
}

static bool parse_fbits(const char *in, int *out) {
  return parse_one_of(in, out,
                      {
                          {"-", 0},
                          {"q7", 7},
                          {"q15", 15},
                      });
}

static bool parse_types(const char *in, type_triple *out) {
  return parse_one_of(in, out,
                      {
                          {"ppp", type_triple::ppp},
                          {"jjj", type_triple::jjj},
                          {"ccc", type_triple::ccc},
                          {"zzz", type_triple::zzz},
                          {"ppr", type_triple::ppr},
                          {"jjh", type_triple::jjh},
                          {"ccs", type_triple::ccs},
                          {"zzd", type_triple::zzd},
                          {"rpp", type_triple::rpp},
                          {"hjj", type_triple::hjj},
                          {"scc", type_triple::scc},
                          {"dzz", type_triple::dzz},
                      });
}

static bool parse_twid(const char *in, wfta::twiddleness *out) {
  return parse_one_of(in, out,
                      {{"n", wfta::twiddleness::none},
                       {"dit", wfta::twiddleness::dit},
                       {"dif", wfta::twiddleness::dif}});
}

static bool parse_dir(const char *in, plfft_direction_t *out) {
  return parse_one_of(in, out,
                      {
                          {"f", PLFFT_FORWARD},
                          {"b", PLFFT_BACKWARD},
                      });
}

static bool parse_mod(const char *in, io_mods_t *out) {
  return parse_one_of(in, out,
                      {
                          {"-", {out_mods::om_none, in_mods::im_none}},
                          {"ol", {out_mods::halflo, in_mods::im_none}},
                          {"oh", {out_mods::halfhi, in_mods::im_none}},
                          {"il", {out_mods::om_none, in_mods::im_halflo}},
                          {"ih", {out_mods::om_none, in_mods::im_halfhi}},
                          {"j", {out_mods::conj_reverse, in_mods::im_none}},
                      });
}

static bool parse_dist(const char *in, wfta::known_layout_t *out, int n) {
  const auto none = std::nullopt;
  const std::optional<int64_t> one = 1;
  return parse_one_of<wfta::known_layout_t>(
      in, out,
      {
          {"gu", {none, none, none, none, one}},
          {"gs", {none, none, none, none, none}},
          {"us", {none, none, one, none, none}},
          {"uu", {none, none, one, none, one}},
          {"tu", {none, one, n, none, one}},
          {"uun", {one, none, one, none, one}},
      });
}

static bool is_c2r(type_triple types) {
  switch (types) {
  case type_triple::ppr:
  case type_triple::jjh:
  case type_triple::ccs:
  case type_triple::zzd:
    return true;
  default:
    return false;
  }
}

static bool is_r2c(type_triple types) {
  switch (types) {
  case type_triple::rpp:
  case type_triple::hjj:
  case type_triple::scc:
  case type_triple::dzz:
    return true;
  default:
    return false;
  }
}

static io_mods_t get_mods(type_triple types, order_kind order,
                          const io_mods_t &modifiers) {
  // modify the modifiers...
  auto [out_mod, in_mod] = modifiers;
  if (is_c2r(types)) {
    // the last kernel run in a c2r transform is marked om_real
    return {out_mods::om_real, in_mod};
  } else if (is_r2c(types)) {
    // the first kernel run in a r2c transform is marked im_real
    return {out_mod, in_mods::im_real};
  } else {
    return modifiers;
  }
}

static constexpr int pack(type_triple types, int fbits = 0) {
  return (static_cast<int>(types) << 8) | (fbits & 0xff);
}

static bool has_fp16_compute(type_triple types, int fbits) {
  if (fbits != 0) {
    return false;
  }
  return types == type_triple::jjj || types == type_triple::jjh ||
         types == type_triple::hjj;
}

int main(int argc, char *argv[]) {
  wfta::target_t target;
  order_kind order;
  int n;
  int fbits;
  type_triple types;
  wfta::twiddleness twid;
  plfft_direction_t dir;
  io_mods_t mod;
  wfta::known_layout_t layout;
  std::optional<std::string> output_path;

  int pos_start = 1;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg[0] != '-') {
      pos_start = i;
      break;
    }

    if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      return EXIT_SUCCESS;
    } else if (arg == "-o") {
      output_path = argv[++i];
    } else if (arg.compare(0, 8, "--output") == 0) {
      // Handle --output VALUE (next argv) and --output=VALUE (same argv).
      const auto eq_pos = arg.find("=");
      output_path =
          eq_pos != std::string::npos ? arg.substr(eq_pos + 1) : argv[++i];
    } else {
      usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (argc - pos_start != 9 || !parse_target(argv[pos_start + 0], &target) ||
      !parse_order(argv[pos_start + 1], &order) ||
      !parse_n(argv[pos_start + 2], &n) ||
      !parse_fbits(argv[pos_start + 3], &fbits) ||
      !parse_types(argv[pos_start + 4], &types) ||
      !parse_twid(argv[pos_start + 5], &twid) ||
      !parse_dir(argv[pos_start + 6], &dir) ||
      !parse_mod(argv[pos_start + 7], &mod) ||
      // parse_dist signature means dist must be parsed after n
      !parse_dist(argv[pos_start + 8], &layout, n)) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((is_c2r(types) || is_r2c(types)) && order != order_kind::ORDER_NA) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if (target.name == "neon" && has_fp16_compute(types, fbits)) {
    fprintf(stderr, "Error: use asimdhp for neon+fp16 kernel, not neon\n");
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  const auto mods = get_mods(types, order, mod);

  const wfta::options_t opts{.target = target,
                             .want_map_file = false,
                             .want_check_kernel_hash = false,
                             .want_kernel_src = true,
                             .want_sme = target.has_sme,
                             .src_path = output_path};

  using h = __fp16;
  using s = float;
  using d = double;
  using j = std::complex<__fp16>;
  using c = std::complex<float>;
  using z = std::complex<double>;

  // clang-format off
  switch (pack(types, fbits)) {
  // c2c
  case pack(type_triple::jjj):
    generate_kernel<j, j>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::ccc):
    generate_kernel<c, c>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::zzz):
    generate_kernel<z, z>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::ppp, 7):
    generate_kernel<cq<7>, cq<7>>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::jjj, 15):
    generate_kernel<cq<15>, cq<15>>(opts, n, dir, twid, order, layout, mods);
    break;

  // c2r
  case pack(type_triple::jjh):
    generate_kernel<j, h>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::ccs):
    generate_kernel<c, s>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::zzd):
    generate_kernel<z, d>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::ppr, 7):
    generate_kernel<cq<7>, q<7>>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::jjh, 15):
    generate_kernel<cq<15>, q<15>>(opts, n, dir, twid, order, layout, mods);
    break;

  // r2c
  case pack(type_triple::hjj):
    generate_kernel<h, j>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::scc):
    generate_kernel<s, c>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::dzz):
    generate_kernel<d, z>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::rpp, 7):
    generate_kernel<q<7>, cq<7>>(opts, n, dir, twid, order, layout, mods);
    break;
  case pack(type_triple::hjj, 15):
    generate_kernel<q<15>, cq<15>>(opts, n, dir, twid, order, layout, mods);
    break;

  default:
    fprintf(stderr, "Error: unsupported fbits, type triple combination: %s %s\n\n", argv[pos_start + 3], argv[pos_start + 4]);
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  // clang-format on

  return EXIT_SUCCESS;
}
