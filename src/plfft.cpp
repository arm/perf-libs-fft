/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft.h"

#include "arm_fft1d_impl.hpp"
#include "cpu_features.hpp"
#include "kernel_provider_capabilities.hpp"
#include "plfft_util.hpp"

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>

enum transform_kind_t {
  TRANSFORM_C2C = 0,
  TRANSFORM_R2C = 1,
  TRANSFORM_C2R = 2,
  TRANSFORM_R2R = 3
};

union transform_payload_t {
  plfft_r2r_kind_t r2r_kind;
  plfft_direction_t c2c_direction;
};

struct plfft_config_t {
  int64_t n;
  int64_t howmany;
  int64_t istride;
  int64_t idist;
  int64_t ostride;
  int64_t odist;
  transform_kind_t transform_kind;
  transform_payload_t transform_payload;
  plfft_data_type_t data_type;
  plfft_io_alias_t alias;
  plfft_sme_mode_t sme_mode;
  plfft_rigor_t rigor;
};

template<typename T, typename... Args>
static T *new_handle(Args &&...args) {
  void *storage = std::malloc(sizeof(T));
  if (!storage) {
    return nullptr;
  }
  return new (storage) T{std::forward<Args>(args)...};
}

template<typename T>
static void destroy_handle(T *handle) {
  if (!handle) {
    return;
  }
  handle->~T();
  std::free(handle);
}

static bool valid_config(const plfft_config_t *config) {
  return config;
}

static bool valid_size(const plfft_config_t *config) {
  return config->n > 0;
}

static bool valid_batch(const plfft_config_t *config) {
  if (config->howmany == 1) {
    return config->istride > 0 && config->ostride > 0;
  }
  return config->howmany > 0 && config->istride > 0 && config->idist > 0 &&
         config->ostride > 0 && config->odist > 0;
}

static bool valid_direction(plfft_direction_t direction) {
  return direction == PLFFT_FORWARD || direction == PLFFT_BACKWARD;
}

static bool valid_r2r_kind(plfft_r2r_kind_t kind) {
  switch (kind) {
  case PLFFT_R2R_DCT_1:
  case PLFFT_R2R_DCT_2:
  case PLFFT_R2R_DCT_3:
  case PLFFT_R2R_DCT_4:
  case PLFFT_R2R_DST_1:
  case PLFFT_R2R_DST_2:
  case PLFFT_R2R_DST_3:
  case PLFFT_R2R_DST_4:
  case PLFFT_R2R_DHT:
  case PLFFT_R2R_R2HC:
  case PLFFT_R2R_HC2R:
    return true;
  }
  return false;
}

static bool valid_transform_kind(const plfft_config_t *config) {
  switch (config->transform_kind) {
  case TRANSFORM_C2C:
    return valid_direction(config->transform_payload.c2c_direction);
  case TRANSFORM_R2C:
  case TRANSFORM_C2R:
    return true;
  case TRANSFORM_R2R:
    return valid_r2r_kind(config->transform_payload.r2r_kind);
  }
  return false;
}

static bool valid_alias(const plfft_config_t *config) {
  return config->alias == PLFFT_IO_NO_ALIAS ||
         config->alias == PLFFT_IO_MAY_ALIAS;
}

static bool valid_sme_mode(const plfft_config_t *config) {
  return config->sme_mode == PLFFT_SME_DISABLED ||
         config->sme_mode == PLFFT_SME_ENABLED;
}

static bool want_sme(const plfft_config_t *config) {
  return config->sme_mode == PLFFT_SME_ENABLED;
}

static bool valid_rigor(const plfft_config_t *config) {
  switch (config->rigor) {
  case plfft_rigor_t::PLFFT_ESTIMATE:
  case plfft_rigor_t::PLFFT_MEASURE:
  case plfft_rigor_t::PLFFT_PATIENT:
  case plfft_rigor_t::PLFFT_EXHAUSTIVE:
    return true;
  }
  return false;
}

template<typename T>
static constexpr bool is_fixed_point = std::is_integral_v<T>;

template<typename T>
static constexpr bool is_fixed_point<std::complex<T>> = is_fixed_point<T>;

static double get_planner_target_secs_total(const plfft_config_t *config) {
  const plfft_rigor_t rigor = config->rigor;
  const double log_n = std::log(config->n);
  if (rigor == plfft_rigor_t::PLFFT_ESTIMATE) {
    return 0.0;
  }
  if (log_n < 7.31322038709) { // n < 1500
    return std::exp(-13.4096 + 0.6539 * log_n);
  }
  switch (rigor) {
  case plfft_rigor_t::PLFFT_ESTIMATE:
    return std::exp(-15.9646 + 0.9689 * log_n);
  case plfft_rigor_t::PLFFT_MEASURE:
    return std::exp(-15.5578 + 0.9562 * log_n);
  case plfft_rigor_t::PLFFT_PATIENT:
    return std::exp(-9.8701 + 0.7029 * log_n);
  case plfft_rigor_t::PLFFT_EXHAUSTIVE:
    return std::exp(-5.0776 + 0.5344 * log_n);
  }
  return 0.0;
}

static double get_planner_margin(const plfft_config_t *config) {
  const plfft_rigor_t rigor = config->rigor;
  switch (rigor) {
  case plfft_rigor_t::PLFFT_ESTIMATE:
    return 0.01;
  case plfft_rigor_t::PLFFT_MEASURE:
    return 0.02;
  case plfft_rigor_t::PLFFT_PATIENT:
    return 0.05;
  case plfft_rigor_t::PLFFT_EXHAUSTIVE:
    return 0.5;
  }
  return 0.0;
}

template<typename Tx, typename Ty>
static plfft::fft_plan_ptr make_batched_plan(const plfft_config_t *config,
                                             plfft_direction_t direction) {
#ifdef PLFFT_ENABLE_SME
  if constexpr (!is_fixed_point<Tx>) {
    if (want_sme(config)) {
      return plfft::make_batched_1d_plan_sme<Tx, Ty>(
          config->n, config->howmany, config->istride, config->idist,
          config->ostride, config->odist, direction, config->alias);
    }
  }
#endif
  return plfft::make_batched_1d_plan<Tx, Ty>(
      config->n, config->howmany, config->istride, config->idist,
      config->ostride, config->odist, (int)direction, config->alias,
      (plfft_r2r_kind_t)0, get_planner_target_secs_total(config),
      get_planner_margin(config));
}

template<typename T>
static plfft::fft_plan_ptr make_c2c_plan(const plfft_config_t *config) {
  using complex_t = std::complex<T>;
  return make_batched_plan<complex_t, complex_t>(
      config, config->transform_payload.c2c_direction);
}

template<typename T>
static plfft::fft_plan_ptr make_r2c_plan(const plfft_config_t *config) {
  using complex_t = std::complex<T>;
  return make_batched_plan<T, complex_t>(config, PLFFT_FORWARD);
}

template<typename T>
static plfft::fft_plan_ptr make_c2r_plan(const plfft_config_t *config) {
  using complex_t = std::complex<T>;
  return make_batched_plan<complex_t, T>(config, PLFFT_BACKWARD);
}

template<typename T>
static plfft::fft_plan_ptr make_r2r_plan(const plfft_config_t *config) {
  return plfft::make_batched_1d_r2r_plan<T>(
      config->n, config->howmany, config->istride, config->idist,
      config->ostride, config->odist, config->transform_payload.r2r_kind,
      config->alias);
}

template<typename T>
static plfft::fft_plan_ptr make_plan(const plfft_config_t *config) {
  switch (config->transform_kind) {
  case TRANSFORM_C2C:
    return make_c2c_plan<T>(config);
  case TRANSFORM_R2C:
    return make_r2c_plan<T>(config);
  case TRANSFORM_C2R:
    return make_c2r_plan<T>(config);
  case TRANSFORM_R2R:
    return make_r2r_plan<T>(config);
  }
  return nullptr;
}

template<typename T>
static plfft::fft_plan_ptr make_fixed_point_plan(const plfft_config_t *config) {
  switch (config->transform_kind) {
  case TRANSFORM_C2C:
    return make_c2c_plan<T>(config);
  case TRANSFORM_R2C:
    return make_r2c_plan<T>(config);
  case TRANSFORM_C2R:
    return make_c2r_plan<T>(config);
  case TRANSFORM_R2R:
    return nullptr;
  }
  return nullptr;
}

const char *plfft_status_to_string(plfft_status_t status) {
  switch (status) {
  case PLFFT_OK:
    return "success";
  case PLFFT_NULL_ARGUMENT:
    return "null argument";
  case PLFFT_INVALID_SIZE:
    return "invalid transform size";
  case PLFFT_UNSUPPORTED_BATCH:
    return "Unsupported batch or stride configuration";
  case PLFFT_INVALID_TRANSFORM_KIND:
    return "invalid transform kind";
  case PLFFT_INVALID_DATA_TYPE:
    return "invalid data type";
  case PLFFT_INVALID_IO_ALIAS:
    return "invalid I/O alias mode";
  case PLFFT_PLAN_CREATION_FAILED:
    return "plan creation failed";
  case PLFFT_INVALID_SME_MODE:
    return "invalid SME mode";
  case PLFFT_SME_NOT_ENABLED:
    return "SME support was not enabled when building PLFFT";
  case PLFFT_SME_UNSUPPORTED_R2R:
    return "SME is not supported for real-to-real transforms";
  case PLFFT_FP16_NOT_AVAILABLE:
    return "native FP16 support is required but unavailable on this CPU";
  case PLFFT_SME_NOT_AVAILABLE:
    return "SME support is unavailable on this CPU";
  case PLFFT_SME_UNSUPPORTED_FIXED_POINT:
    return "SME is not supported for fixed-point transforms";
  case PLFFT_INVALID_PLANNER_RIGOR:
    return "invalid planning rigor level";
  }
  return "unknown status";
}

[[noreturn]] static void out_of_memory() {
  std::fputs("Fatal error: out of memory\n", stderr);
  std::abort();
}

static plfft_status_t create_config(plfft_config_t **config, int64_t n,
                                    transform_kind_t transform_kind,
                                    transform_payload_t transform_payload) {
  if (config == nullptr) {
    return PLFFT_NULL_ARGUMENT;
  }
  *config = nullptr;

  *config = new_handle<plfft_config_t>(n, int64_t{1}, int64_t{1}, n, int64_t{1},
                                       n, transform_kind, transform_payload,
                                       PLFFT_DATA_TYPE_FP32, PLFFT_IO_NO_ALIAS,
                                       PLFFT_SME_DISABLED, PLFFT_MEASURE);
  if (*config == nullptr) {
    out_of_memory();
  }
  return PLFFT_OK;
}

plfft_status_t plfft_config_create(plfft_config_t **config, int64_t n,
                                   plfft_direction_t direction) {
  if (config == nullptr) {
    return PLFFT_NULL_ARGUMENT;
  }
  *config = nullptr;

  if (!valid_direction(direction)) {
    return PLFFT_INVALID_TRANSFORM_KIND;
  }

  return create_config(config, n, TRANSFORM_C2C,
                       transform_payload_t{.c2c_direction = direction});
}

plfft_status_t plfft_config_create_r2c(plfft_config_t **config, int64_t n) {
  return create_config(config, n, TRANSFORM_R2C, transform_payload_t{});
}

plfft_status_t plfft_config_create_c2r(plfft_config_t **config, int64_t n) {
  return create_config(config, n, TRANSFORM_C2R, transform_payload_t{});
}

plfft_status_t plfft_config_create_r2r(plfft_config_t **config, int64_t n,
                                       plfft_r2r_kind_t r2r_kind) {
  if (config == nullptr) {
    return PLFFT_NULL_ARGUMENT;
  }
  *config = nullptr;

  if (!valid_r2r_kind(r2r_kind)) {
    return PLFFT_INVALID_TRANSFORM_KIND;
  }

  return create_config(config, n, TRANSFORM_R2R,
                       transform_payload_t{.r2r_kind = r2r_kind});
}

void plfft_config_destroy(plfft_config_t *config) {
  destroy_handle(config);
}

void plfft_config_set_data_type(plfft_config_t *config,
                                plfft_data_type_t data_type) {
  config->data_type = data_type;
}

plfft_data_type_t plfft_config_get_data_type(const plfft_config_t *config) {
  return config->data_type;
}

void plfft_config_set_io_alias(plfft_config_t *config, plfft_io_alias_t alias) {
  config->alias = alias;
}

plfft_io_alias_t plfft_config_get_io_alias(const plfft_config_t *config) {
  return config->alias;
}

void plfft_config_set_sme_mode(plfft_config_t *config,
                               plfft_sme_mode_t sme_mode) {
  config->sme_mode = sme_mode;
}

plfft_sme_mode_t plfft_config_get_sme_mode(const plfft_config_t *config) {
  return config->sme_mode;
}

void plfft_config_set_batch(plfft_config_t *config, int64_t howmany,
                            int64_t istride, int64_t idist, int64_t ostride,
                            int64_t odist) {
  config->howmany = howmany;
  config->istride = istride;
  config->idist = idist;
  config->ostride = ostride;
  config->odist = odist;
}

int64_t plfft_config_get_howmany(const plfft_config_t *config) {
  return config->howmany;
}

int64_t plfft_config_get_istride(const plfft_config_t *config) {
  return config->istride;
}

int64_t plfft_config_get_idist(const plfft_config_t *config) {
  return config->idist;
}

int64_t plfft_config_get_ostride(const plfft_config_t *config) {
  return config->ostride;
}

int64_t plfft_config_get_odist(const plfft_config_t *config) {
  return config->odist;
}

void plfft_config_set_rigor(plfft_config_t *config, const plfft_rigor_t rigor) {
  config->rigor = rigor;
}

plfft_rigor_t plfft_config_get_rigor(const plfft_config_t *config) {
  return config->rigor;
}

plfft_status_t plfft_plan_create_from_config(plfft_plan_t **plan,
                                             const plfft_config_t *config) {
  if (!plan) {
    return PLFFT_NULL_ARGUMENT;
  }

  *plan = nullptr;

  if (!valid_config(config)) {
    return PLFFT_NULL_ARGUMENT;
  }

  if (!valid_size(config)) {
    return PLFFT_INVALID_SIZE;
  }

  if (!valid_batch(config)) {
    return PLFFT_UNSUPPORTED_BATCH;
  }

  if (!valid_transform_kind(config)) {
    return PLFFT_INVALID_TRANSFORM_KIND;
  }

  if (!valid_alias(config)) {
    return PLFFT_INVALID_IO_ALIAS;
  }

  if (!valid_sme_mode(config)) {
    return PLFFT_INVALID_SME_MODE;
  }

  if (want_sme(config) && config->transform_kind == TRANSFORM_R2R) {
    return PLFFT_SME_UNSUPPORTED_R2R;
  }

  if (want_sme(config) && (config->data_type == PLFFT_DATA_TYPE_Q7 ||
                           config->data_type == PLFFT_DATA_TYPE_Q15)) {
    return PLFFT_SME_UNSUPPORTED_FIXED_POINT;
  }

#ifndef PLFFT_ENABLE_SME
  if (want_sme(config)) {
    return PLFFT_SME_NOT_ENABLED;
  }
#else
  if (want_sme(config) && !plfft::get_cpu_features().sme) {
    return PLFFT_SME_NOT_AVAILABLE;
  }
#endif

  if (config->data_type == PLFFT_DATA_TYPE_FP16 &&
      !plfft::get_cpu_features().asimdhp &&
      !plfft::kernel_provider_emulates_fp16()) {
    return PLFFT_FP16_NOT_AVAILABLE;
  }

  if (!valid_rigor(config)) {
    return PLFFT_INVALID_PLANNER_RIGOR;
  }

  plfft::fft_plan_ptr fft_plan;
  switch (config->data_type) {
  case PLFFT_DATA_TYPE_FP16:
    fft_plan = make_plan<half>(config);
    break;
  case PLFFT_DATA_TYPE_FP32:
    fft_plan = make_plan<float>(config);
    break;
  case PLFFT_DATA_TYPE_FP64:
    fft_plan = make_plan<double>(config);
    break;
#if PLFFT_ENABLE_FIXED_POINT
  case PLFFT_DATA_TYPE_Q7:
    fft_plan = make_fixed_point_plan<int8_t>(config);
    break;
  case PLFFT_DATA_TYPE_Q15:
    fft_plan = make_fixed_point_plan<int16_t>(config);
    break;
#endif // PLFFT_ENABLE_FIXED_POINT
  default:
    return PLFFT_INVALID_DATA_TYPE;
  }

  if (!fft_plan) {
    return PLFFT_PLAN_CREATION_FAILED;
  }

  *plan = reinterpret_cast<plfft_plan_t *>(fft_plan.release());
  return PLFFT_OK;
}

void plfft_plan_destroy(plfft_plan_t *plan) {
  destroy_handle(reinterpret_cast<plfft::fft_plan *>(plan));
}

void plfft_plan_execute(const plfft_plan_t *plan, const void *input,
                        void *output) {
  reinterpret_cast<const plfft::fft_plan *>(plan)->execute(input, output);
}
