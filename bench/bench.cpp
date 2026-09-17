/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "bench_pmu.hpp"
#include "plfft.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace {

template<class T, size_t Alignment>
struct AlignedAllocator {
  using value_type = T;

  template<class U>
  struct rebind {
    using other = AlignedAllocator<U, Alignment>;
  };

  T *allocate(std::size_t n) {
    void *ptr = nullptr;
    const std::size_t size = n * sizeof(T);
    const std::size_t rounded_size =
        ((size + Alignment - 1) / Alignment) * Alignment;
#ifdef _WIN32
    // MS CRT does not provide std::aligned_alloc, instead use _aligned_malloc
    ptr = _aligned_malloc(rounded_size, Alignment);
#else
    ptr = std::aligned_alloc(Alignment, rounded_size);
#endif
    assert(ptr && "Could not allocate array\n");
    return static_cast<T *>(ptr);
  }

  void deallocate(T *ptr, std::size_t) noexcept {
#ifdef _WIN32
    // MS CRT free cannot handle aligned allocations, we have to use
    // _aligned_free on Windows
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
  }
};

struct ConfigDeleter {
  void operator()(plfft_config_t *config) const {
    plfft_config_destroy(config);
  }
};

struct PlanDeleter {
  void operator()(plfft_plan_t *plan) const {
    plfft_plan_destroy(plan);
  }
};

using ConfigPtr = std::unique_ptr<plfft_config_t, ConfigDeleter>;
using PlanPtr = std::unique_ptr<plfft_plan_t, PlanDeleter>;

enum class TransformKind { c2c, r2c, c2r };

struct ParsedOptions {
  std::optional<int64_t> n;
  std::optional<TransformKind> transform_kind;
  std::optional<plfft_data_type_t> dtype;
  std::optional<plfft_direction_t> direction;
  std::optional<plfft_io_alias_t> io_alias;
  std::optional<int64_t> howmany;
  std::optional<std::string> layout;
  std::optional<int64_t> istride;
  std::optional<int64_t> idist;
  std::optional<int64_t> ostride;
  std::optional<int64_t> odist;
  std::optional<int64_t> niters;
  std::optional<int64_t> warmup_ms;
  std::optional<PMUMode> pmu;
  std::optional<plfft_rigor_t> rigor;
  bool sme = false;
};

struct BenchmarkOptions {
  int64_t n;
  TransformKind transform_kind;
  plfft_data_type_t dtype;
  plfft_direction_t direction;
  plfft_io_alias_t io_alias;
  int64_t howmany;
  int64_t istride;
  int64_t idist;
  int64_t ostride;
  int64_t odist;
  int64_t niters;
  int64_t warmup_ms;
  std::optional<PMUMode> pmu;
  plfft_rigor_t rigor;
  bool sme;
};

enum class ParseResult {
  ok,
  usage,
  layout_conflict,
};

int exit_usage(const std::string &program_name) {
  // clang-format off
  std::cerr << "Usage: " << program_name << " --n <size> [options]\n\n"
            << "Options:\n"
            << "  --n, -n <size>         transform length\n"
            << "  --transform-kind, -k <kind> c2c | r2c | c2r (default: c2c)\n"
            << "  --data-type, -p <type> f16 | f32 | f64 | q7 | q15 (default: f32)\n"
            << "  --direction, -d <dir>  forward | f | backward | b (default: forward for c2c; inferred for r2c/c2r)\n"
            << "  --io-alias, -a <alias> no | may (default: no)\n"
            << "  --howmany, -H <count>  (default: 1)\n"
            << "  --layout, -l <layout>  uu | tu | ut | tt\n"
            << "  --istride, --is <s>    input stride (default: 1)\n"
            << "  --ostride, --os <s>    output stride (default: 1)\n"
            << "  --idist, --id <d>      input distance (default: logical input length * istride)\n"
            << "  --odist, --od <d>      output distance (default: logical output length * ostride)\n"
            << "  --niters <count>       number of timed executions (default: 10)\n"
            << "  --warmup-ms <ms>       warm-up duration in milliseconds (default: 50)\n"
            << "  --pmu <mode>           mark the measured region with the selected PMU "
            <<                          "implementation and report average cycles\n"
            << "                         allowed values: " << pmu_mode_values() << "\n"
            << "  --sme                  allow SME kernels\n"
            << "  --rigor, -r <level>    planning rigor: estimate | s | measure | m | patient | p | exhaustive | x (default: estimate)\n"
            << "  --help, -h             show this help text\n";
  // clang-format on
  exit(EXIT_FAILURE);
}

template<typename T>
std::optional<T>
parse_one_of(const std::string &in,
             const std::unordered_map<std::string, T> &choices) {
  auto it = choices.find(in);
  if (it == choices.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<int64_t> parse_int64(const std::string &in) {
  char *end = nullptr;
  const long long val = std::strtoll(in.c_str(), &end, 10);
  if (end == in.c_str() || *end != '\0') {
    return std::nullopt;
  }
  return static_cast<int64_t>(val);
}

std::optional<int64_t> parse_int64_gt_zero(const std::string &in) {
  const auto value = parse_int64(in);
  if (!value || *value <= 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<int64_t> parse_int64_ge_zero(const std::string &in) {
  const auto value = parse_int64(in);
  if (!value || *value < 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<plfft_data_type_t> parse_dtype(const std::string &in) {
  return parse_one_of<plfft_data_type_t>(in, {{"f16", PLFFT_DATA_TYPE_FP16},
                                              {"f32", PLFFT_DATA_TYPE_FP32},
                                              {"f64", PLFFT_DATA_TYPE_FP64},
                                              {"q7", PLFFT_DATA_TYPE_Q7},
                                              {"q15", PLFFT_DATA_TYPE_Q15}});
}

std::optional<TransformKind> parse_transform_kind(const std::string &in) {
  return parse_one_of<TransformKind>(in, {{"c2c", TransformKind::c2c},
                                          {"r2c", TransformKind::r2c},
                                          {"c2r", TransformKind::c2r}});
}

std::optional<plfft_direction_t> parse_direction(const std::string &in) {
  return parse_one_of<plfft_direction_t>(in, {{"forward", PLFFT_FORWARD},
                                              {"f", PLFFT_FORWARD},
                                              {"backward", PLFFT_BACKWARD},
                                              {"b", PLFFT_BACKWARD}});
}

std::optional<plfft_io_alias_t> parse_io_alias(const std::string &in) {
  return parse_one_of<plfft_io_alias_t>(
      in, {{"no", PLFFT_IO_NO_ALIAS}, {"may", PLFFT_IO_MAY_ALIAS}});
}

bool parse_layout(const std::string &in) {
  return in == "uu" || in == "tu" || in == "ut" || in == "tt";
}

std::optional<plfft_rigor_t> parse_rigor(const std::string &in) {
  return parse_one_of<plfft_rigor_t>(in, {{"estimate", PLFFT_ESTIMATE},
                                          {"s", PLFFT_ESTIMATE},
                                          {"measure", PLFFT_MEASURE},
                                          {"m", PLFFT_MEASURE},
                                          {"patient", PLFFT_PATIENT},
                                          {"p", PLFFT_PATIENT},
                                          {"exhaustive", PLFFT_EXHAUSTIVE},
                                          {"x", PLFFT_EXHAUSTIVE}});
}

bool has_stride_or_dist_arg(const ParsedOptions &options) {
  return options.istride || options.idist || options.ostride || options.odist;
}

ParseResult parse_args(int argc, char **argv, ParsedOptions *options) {
  for (int i = 1; i < argc; ++i) {
    std::string key = argv[i];
    if (key == "--help" || key == "-h") {
      return ParseResult::usage;
    }
    if (key == "--sme") {
      options->sme = true;
      continue;
    }
    if (key.rfind("-", 0) != 0) {
      return ParseResult::usage;
    }

    std::string value_str;
    const auto eq_pos = key.find('=');
    if (eq_pos != std::string::npos) {
      value_str = key.substr(eq_pos + 1);
      key = key.substr(0, eq_pos);
      if (value_str.empty()) {
        return ParseResult::usage;
      }
    } else {
      if (i + 1 >= argc) {
        return ParseResult::usage;
      }
      value_str = argv[++i];
    }

    if (key == "--pmu") {
      options->pmu = parse_pmu_mode(value_str);
      if (!options->pmu) {
        return ParseResult::usage;
      }
    } else if (key == "--n" || key == "-n") {
      options->n = parse_int64_gt_zero(value_str);
      if (!options->n) {
        return ParseResult::usage;
      }
    } else if (key == "--transform-kind" || key == "-k") {
      options->transform_kind = parse_transform_kind(value_str);
      if (!options->transform_kind) {
        return ParseResult::usage;
      }
    } else if (key == "--data-type" || key == "-p") {
      options->dtype = parse_dtype(value_str);
      if (!options->dtype) {
        return ParseResult::usage;
      }
    } else if (key == "--direction" || key == "-d") {
      options->direction = parse_direction(value_str);
      if (!options->direction) {
        return ParseResult::usage;
      }
    } else if (key == "--io-alias" || key == "-a") {
      options->io_alias = parse_io_alias(value_str);
      if (!options->io_alias) {
        return ParseResult::usage;
      }
    } else if (key == "--howmany" || key == "-H") {
      options->howmany = parse_int64_gt_zero(value_str);
      if (!options->howmany) {
        return ParseResult::usage;
      }
    } else if (key == "--layout" || key == "-l") {
      if (!parse_layout(value_str)) {
        return ParseResult::usage;
      }
      options->layout = value_str;
    } else if (key == "--istride" || key == "--is") {
      options->istride = parse_int64_gt_zero(value_str);
      if (!options->istride) {
        return ParseResult::usage;
      }
    } else if (key == "--idist" || key == "--id") {
      options->idist = parse_int64_gt_zero(value_str);
      if (!options->idist) {
        return ParseResult::usage;
      }
    } else if (key == "--ostride" || key == "--os") {
      options->ostride = parse_int64_gt_zero(value_str);
      if (!options->ostride) {
        return ParseResult::usage;
      }
    } else if (key == "--odist" || key == "--od") {
      options->odist = parse_int64_gt_zero(value_str);
      if (!options->odist) {
        return ParseResult::usage;
      }
    } else if (key == "--niters") {
      options->niters = parse_int64_gt_zero(value_str);
      if (!options->niters) {
        return ParseResult::usage;
      }
    } else if (key == "--warmup-ms") {
      options->warmup_ms = parse_int64_ge_zero(value_str);
      if (!options->warmup_ms) {
        return ParseResult::usage;
      }
    } else if (key == "--rigor" || key == "-r") {
      options->rigor = parse_rigor(value_str);
      if (!options->rigor) {
        return ParseResult::usage;
      }
    } else {
      return ParseResult::usage;
    }
  }

  if (!options->n) {
    return ParseResult::usage;
  }
  return ParseResult::ok;
}

ParseResult resolve_options(const ParsedOptions &parsed,
                            BenchmarkOptions *options) {
  const plfft_data_type_t default_dtype = PLFFT_DATA_TYPE_FP32;
  plfft_direction_t default_direction = PLFFT_FORWARD;
  plfft_io_alias_t default_io_alias = PLFFT_IO_NO_ALIAS;
  int64_t default_howmany = 1;
  int64_t default_istride = 1;
  int64_t default_ostride = 1;
  int64_t default_niters = 10;
  int64_t default_warmup_ms = 50;

  options->n = *parsed.n;
  options->transform_kind = parsed.transform_kind.value_or(TransformKind::c2c);
  options->dtype = parsed.dtype.value_or(default_dtype);
  const plfft_direction_t inferred_direction =
      options->transform_kind == TransformKind::c2r ? PLFFT_BACKWARD
                                                    : PLFFT_FORWARD;
  if (options->transform_kind != TransformKind::c2c && parsed.direction &&
      *parsed.direction != inferred_direction) {
    return ParseResult::usage;
  }
  options->direction = parsed.direction.value_or(
      options->transform_kind == TransformKind::c2c ? default_direction
                                                    : inferred_direction);
  options->io_alias = parsed.io_alias.value_or(default_io_alias);
  options->howmany = parsed.howmany.value_or(default_howmany);
  options->niters = parsed.niters.value_or(default_niters);
  options->warmup_ms = parsed.warmup_ms.value_or(default_warmup_ms);
  options->pmu = parsed.pmu;
  options->sme = parsed.sme;
  options->rigor = parsed.rigor.value_or(PLFFT_ESTIMATE);

  const int64_t hermitian_n = options->n / 2 + 1;
  const int64_t input_n =
      options->transform_kind == TransformKind::c2r ? hermitian_n : options->n;
  const int64_t output_n =
      options->transform_kind == TransformKind::r2c ? hermitian_n : options->n;

  if (parsed.layout) {
    if (has_stride_or_dist_arg(parsed)) {
      return ParseResult::layout_conflict;
    }
    if (*parsed.layout == "uu") {
      options->istride = options->howmany;
      options->idist = 1;
      options->ostride = options->howmany;
      options->odist = 1;
    } else if (*parsed.layout == "tu") {
      options->istride = 1;
      options->idist = input_n;
      options->ostride = options->howmany;
      options->odist = 1;
    } else if (*parsed.layout == "ut") {
      options->istride = options->howmany;
      options->idist = 1;
      options->ostride = 1;
      options->odist = output_n;
    } else if (*parsed.layout == "tt") {
      options->istride = 1;
      options->idist = input_n;
      options->ostride = 1;
      options->odist = output_n;
    } else {
      return ParseResult::usage;
    }
    return ParseResult::ok;
  }

  options->istride = parsed.istride.value_or(default_istride);
  options->ostride = parsed.ostride.value_or(default_ostride);
  options->idist = parsed.idist.value_or(input_n * options->istride);
  options->odist = parsed.odist.value_or(output_n * options->ostride);
  return ParseResult::ok;
}

size_t buffer_size(int64_t n, int64_t howmany, int64_t stride, int64_t dist) {
  const int64_t elements = (howmany - 1) * dist + (n - 1) * stride + 1;
  return static_cast<size_t>(elements);
}

template<typename Execute>
void warm_up(int64_t warmup_ms, const Execute &execute) {
  using clock = std::chrono::steady_clock;

  const auto start_time = clock::now();
  const auto duration = std::chrono::milliseconds(warmup_ms);
  while (clock::now() - start_time < duration) {
    execute();
  }
}

template<typename Execute>
double measure_time_ms(int64_t niters, const Execute &execute) {
  using clock = std::chrono::steady_clock;

  const auto start_time = clock::now();
  for (int64_t i = 0; i < niters; ++i) {
    execute();
  }
  const auto end_time = clock::now();

  const std::chrono::duration<double, std::milli> diff_ms =
      end_time - start_time;
  return diff_ms.count() / static_cast<double>(niters);
}

template<typename Execute>
double measure_cycles(int64_t niters, const Execute &execute,
                      PMUMode pmu_mode) {
  pmu_start(pmu_mode);
  for (int64_t i = 0; i < niters; ++i) {
    execute();
  }
  return static_cast<double>(pmu_stop(pmu_mode)) / niters;
}

template<typename Tx, typename Ty>
int run_bench(const BenchmarkOptions &options, const plfft_plan_t *plan) {
  using input_buffer_t = std::vector<Tx, AlignedAllocator<Tx, 1024>>;
  using output_buffer_t = std::vector<Ty, AlignedAllocator<Ty, 1024>>;

  const int64_t hermitian_n = options.n / 2 + 1;
  const int64_t input_n =
      options.transform_kind == TransformKind::c2r ? hermitian_n : options.n;
  const int64_t output_n =
      options.transform_kind == TransformKind::r2c ? hermitian_n : options.n;

  const auto in_size =
      buffer_size(input_n, options.howmany, options.istride, options.idist);
  const auto out_size =
      buffer_size(output_n, options.howmany, options.ostride, options.odist);

  input_buffer_t input;
  output_buffer_t output;
  void *output_ptr;
  if (options.io_alias == PLFFT_IO_MAY_ALIAS) {
    const size_t storage_bytes =
        std::max(in_size * sizeof(Tx), out_size * sizeof(Ty));
    input.resize((storage_bytes + sizeof(Tx) - 1) / sizeof(Tx));
    output_ptr = input.data();
  } else {
    input.resize(in_size);
    output.resize(out_size);
    output_ptr = output.data();
  }
  const void *input_ptr = input.data();

  const auto execute = [&] { plfft_plan_execute(plan, input_ptr, output_ptr); };

  warm_up(options.warmup_ms, execute);

  if (options.pmu) {
    const double average_cycles =
        measure_cycles(options.niters, execute, *options.pmu);
    std::cout << "{\"value\": " << std::setprecision(12) << average_cycles
              << ", \"unit\": \"cycles\"}\n";
  } else {
    const double average_ms = measure_time_ms(options.niters, execute);
    std::cout << "{\"value\": " << std::setprecision(12) << average_ms
              << ", \"unit\": \"ms\"}\n";
  }
  return EXIT_SUCCESS;
}

template<typename T>
int run_bench_for_type(const BenchmarkOptions &options,
                       const plfft_plan_t *plan) {
  using Complex = std::complex<T>;

  switch (options.transform_kind) {
  case TransformKind::c2c:
    return run_bench<Complex, Complex>(options, plan);
  case TransformKind::r2c:
    return run_bench<T, Complex>(options, plan);
  case TransformKind::c2r:
    return run_bench<Complex, T>(options, plan);
  default:
    __builtin_unreachable();
  }
}

void exit_with_error(const std::string &msg) {
  std::cerr << "error: " << msg << '\n';
  exit(EXIT_FAILURE);
}

void exit_if_failure(plfft_status_t status, const std::string &desc) {
  if (status != PLFFT_OK) {
    exit_with_error(desc + ": " + plfft_status_to_string(status));
  }
}

} // namespace

int main(int argc, char **argv) {
  ParsedOptions parsed_options;
  const ParseResult parse_result = parse_args(argc, argv, &parsed_options);
  if (parse_result == ParseResult::usage) {
    exit_usage(argv[0]);
  }

  BenchmarkOptions options;
  const ParseResult resolve_result = resolve_options(parsed_options, &options);
  if (resolve_result == ParseResult::usage) {
    exit_usage(argv[0]);
  }
  if (resolve_result == ParseResult::layout_conflict) {
    exit_with_error(
        "--layout cannot be used with explicit stride or distance options");
  }

  plfft_config_t *config = nullptr;
  plfft_status_t status = PLFFT_INVALID_TRANSFORM_KIND;
  switch (options.transform_kind) {
  case TransformKind::c2c:
    status = plfft_config_create(&config, options.n, options.direction);
    break;
  case TransformKind::r2c:
    status = plfft_config_create_r2c(&config, options.n);
    break;
  case TransformKind::c2r:
    status = plfft_config_create_c2r(&config, options.n);
    break;
  }
  ConfigPtr config_defer_delete(config);
  exit_if_failure(status, "config creation failed");

  plfft_config_set_data_type(config, options.dtype);
  plfft_config_set_io_alias(config, options.io_alias);
  plfft_config_set_sme_mode(config, options.sme ? PLFFT_SME_ENABLED
                                                : PLFFT_SME_DISABLED);
  plfft_config_set_batch(config, options.howmany, options.istride,
                         options.idist, options.ostride, options.odist);
  plfft_config_set_rigor(config, options.rigor);

  plfft_plan_t *plan = nullptr;
  status = plfft_plan_create_from_config(&plan, config);
  PlanPtr plan_defer_delete(plan);
  exit_if_failure(status, "plan creation failed");

  switch (options.dtype) {
  case PLFFT_DATA_TYPE_FP16:
    return run_bench_for_type<__fp16>(options, plan);
  case PLFFT_DATA_TYPE_FP32:
    return run_bench_for_type<float>(options, plan);
  case PLFFT_DATA_TYPE_FP64:
    return run_bench_for_type<double>(options, plan);
#if PLFFT_ENABLE_FIXED_POINT
  case PLFFT_DATA_TYPE_Q7:
    return run_bench_for_type<int8_t>(options, plan);
  case PLFFT_DATA_TYPE_Q15:
    return run_bench_for_type<int16_t>(options, plan);
#endif // PLFFT_ENABLE_FIXED_POINT
  default:
    exit_with_error("unsupported datatype");
  }
}
