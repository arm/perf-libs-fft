/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft.h"
#include "test_utils.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <type_traits>

struct config_delete_defer {
  plfft_config_t *config;

  ~config_delete_defer() {
    plfft_config_destroy(config);
  }
};

struct plan_delete_defer {
  plfft_plan_t *plan;

  ~plan_delete_defer() {
    plfft_plan_destroy(plan);
  }
};

template<typename T>
struct complex_plfft {
  T real;
  T imag;
};

template<typename T>
struct plfft_data_type;

template<>
struct plfft_data_type<float> {
  static constexpr plfft_data_type_t value = PLFFT_DATA_TYPE_FP32;
};

template<>
struct plfft_data_type<double> {
  static constexpr plfft_data_type_t value = PLFFT_DATA_TYPE_FP64;
};

template<>
struct plfft_data_type<__fp16> {
  static constexpr plfft_data_type_t value = PLFFT_DATA_TYPE_FP16;
};

#if PLFFT_ENABLE_FIXED_POINT
template<>
struct plfft_data_type<int8_t> {
  static constexpr plfft_data_type_t value = PLFFT_DATA_TYPE_Q7;
};

template<>
struct plfft_data_type<int16_t> {
  static constexpr plfft_data_type_t value = PLFFT_DATA_TYPE_Q15;
};
#endif

template<typename T>
static constexpr bool is_fixed_point = std::is_integral_v<T>;

template<typename T>
static complex_plfft<T> make_complex(double real, double imag) {
  return {convert_to<T>{}(real), convert_to<T>{}(imag)};
}

template<typename T>
static bool equals_expected(T actual, double expected, size_t n) {
  if constexpr (is_fixed_point<T>) {
    // Fixed-point FFTs scale by 1/n and may round by one output LSB.
    const T expected_fixed = convert_to<T>{}(expected / static_cast<double>(n));
    return std::abs(static_cast<int64_t>(actual) -
                    static_cast<int64_t>(expected_fixed)) <= 1;
  }
  return static_cast<double>(actual) == expected;
}

static double value_greater_than_one(size_t i) {
  return 1.0 + static_cast<double>(i);
}

static double values_representable_in_q7(size_t i) {
  // Each input and the unscaled n=8 sum fit in Q0.7.
  static constexpr std::array<double, 4> values{1.0 / 4.0, 1.0 / 8.0,
                                                1.0 / 16.0, 1.0 / 32.0};
  return values[i % values.size()];
}

static double values_representable_in_q15(size_t i) {
  static constexpr std::array<double, 4> values{1.0 / 256.0, 1.0 / 512.0,
                                                1.0 / 1024.0, 1.0 / 1024.0};
  return values[i % values.size()];
}

// Verify that the first C2C output equals the sum of the input.
template<typename T>
void test_c2c_first_output_is_sum_of_input(double (*input_value)(size_t)) {
  const size_t n = 8;
  complex_plfft<T> input[8];
  complex_plfft<T> output[8];
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;
  plfft_status_t status = PLFFT_OK;
  double sum = 0.0;

  for (size_t i = 0; i < n; ++i) {
    input[i] = make_complex<T>(input_value(i), 0.0);
    output[i] = make_complex<T>(0.0, 0.0);
    sum += input_value(i);
  }

  status = plfft_config_create(&config, n, PLFFT_FORWARD);
  config_delete_defer config_deleter{config};
  REQUIRE(status == PLFFT_OK, "Error creating C2C config");
  REQUIRE(config != nullptr, "Config should not be null");

  plfft_config_set_data_type(config, plfft_data_type<T>::value);
  REQUIRE(plfft_config_get_data_type(config) == plfft_data_type<T>::value,
          "Precision setter/getter mismatch");
  plfft_config_set_io_alias(config, PLFFT_IO_NO_ALIAS);

  status = plfft_plan_create_from_config(&plan, config);
  plan_delete_defer plan_deleter{plan};
  REQUIRE(status == PLFFT_OK, "Error creating C2C plan");

  plfft_plan_execute(plan, input, output);

  REQUIRE(equals_expected(output[0].real, sum, n) &&
              equals_expected(output[0].imag, 0.0, n),
          "C2C first output should match input sum");
}

// Verify that the first R2C output equals the sum of the real input.
template<typename T>
void test_r2c_first_output_is_sum_of_input(double (*input_value)(size_t)) {
  const size_t n = 8;
  T input[8];
  complex_plfft<T> spectrum[5];
  plfft_config_t *config_r2c = nullptr;
  plfft_plan_t *plan_r2c = nullptr;
  plfft_status_t status = PLFFT_OK;
  double sum = 0.0;

  for (size_t i = 0; i < n; ++i) {
    input[i] = convert_to<T>{}(input_value(i));
    sum += input_value(i);
  }
  for (size_t i = 0; i < n / 2 + 1; ++i) {
    spectrum[i] = make_complex<T>(0.0, 0.0);
  }

  status = plfft_config_create_r2c(&config_r2c, n);
  config_delete_defer config_r2c_deleter{config_r2c};
  REQUIRE(status == PLFFT_OK, "Error creating R2C config");
  REQUIRE(config_r2c != nullptr, "Config should not be null");

  plfft_config_set_data_type(config_r2c, plfft_data_type<T>::value);

  status = plfft_plan_create_from_config(&plan_r2c, config_r2c);
  plan_delete_defer plan_r2c_deleter{plan_r2c};
  REQUIRE(status == PLFFT_OK, "Error creating R2C plan");

  plfft_plan_execute(plan_r2c, input, spectrum);

  REQUIRE(equals_expected(spectrum[0].real, sum, n) &&
              equals_expected(spectrum[0].imag, 0.0, n),
          "R2C first output should match input sum");
}

// Verify the first C2R output for a Hermitian half-spectrum.
template<typename T>
void test_c2r_first_output_is_sum_of_hermitian_input(
    double (*input_value)(size_t)) {
  const size_t n = 8;
  complex_plfft<T> spectrum[5];
  T output[8] = {};
  plfft_config_t *config_c2r = nullptr;
  plfft_plan_t *plan_c2r = nullptr;
  plfft_status_t status = PLFFT_OK;

  for (size_t i = 0; i < n / 2 + 1; ++i) {
    spectrum[i] = make_complex<T>(input_value(i), 0.0);
  }

  double sum = input_value(0) + input_value(n / 2);
  for (size_t i = 1; i < n / 2; ++i) {
    sum += 2.0 * input_value(i);
  }

  status = plfft_config_create_c2r(&config_c2r, n);
  config_delete_defer config_c2r_deleter{config_c2r};
  REQUIRE(status == PLFFT_OK, "Error creating C2R config");
  REQUIRE(config_c2r != nullptr, "Config should not be null");

  plfft_config_set_data_type(config_c2r, plfft_data_type<T>::value);

  status = plfft_plan_create_from_config(&plan_c2r, config_c2r);
  plan_delete_defer plan_c2r_deleter{plan_c2r};
  REQUIRE(status == PLFFT_OK, "Error creating C2R plan");

  plfft_plan_execute(plan_c2r, spectrum, output);

  REQUIRE(equals_expected(output[0], sum, n),
          "C2R first output should match Hermitian input sum");
}

// Verify that the first R2HC output equals the sum of the input.
template<typename T>
void test_r2r_first_output_is_sum_of_input(double (*input_value)(size_t)) {
  const size_t n = 8;
  T input[8];
  T output[8];
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;
  plfft_status_t status = PLFFT_OK;
  double sum = 0.0;

  for (size_t i = 0; i < n; ++i) {
    input[i] = convert_to<T>{}(input_value(i));
    output[i] = convert_to<T>{}(0.0);
    sum += input_value(i);
  }

  status = plfft_config_create_r2r(&config, n, PLFFT_R2R_R2HC);
  config_delete_defer config_deleter{config};
  REQUIRE(status == PLFFT_OK, "Error creating R2R config");
  REQUIRE(config != nullptr, "Config should not be null");

  plfft_config_set_data_type(config, plfft_data_type<T>::value);

  status = plfft_plan_create_from_config(&plan, config);
  plan_delete_defer plan_deleter{plan};
  REQUIRE(status == PLFFT_OK, "Error creating R2R plan");

  plfft_plan_execute(plan, input, output);

  REQUIRE(equals_expected(output[0], sum, n),
          "R2R first output should match input sum");
}

// Exercise every transform family supported by a scalar type.
template<typename T>
void test_transforms(double (*input_value)(size_t)) {
  test_c2c_first_output_is_sum_of_input<T>(input_value);
  test_r2c_first_output_is_sum_of_input<T>(input_value);
  test_c2r_first_output_is_sum_of_hermitian_input<T>(input_value);

  // Fixed-point R2R plans are not supported.
  if constexpr (!is_fixed_point<T>) {
    test_r2r_first_output_is_sum_of_input<T>(input_value);
  }
}

// Exercise a data type with input ranges it can represent.
template<typename T>
void test_data_type() {
  // Only dispatch to input ranges that make sense for the data-type.
  if constexpr (!is_fixed_point<T>) {
    test_transforms<T>(value_greater_than_one);
  }
  test_transforms<T>(values_representable_in_q7);

  if constexpr (!std::is_same_v<T, int8_t>) {
    // These finer values quantize to zero in Q0.7.
    test_transforms<T>(values_representable_in_q15);
  }
}

// Verify the values assigned to a newly created configuration.
void test_config_defaults() {
  int64_t n = 16;
  plfft_config_t *config = nullptr;
  plfft_config_create(&config, n, PLFFT_FORWARD);
  config_delete_defer config_deleter{config};

  REQUIRE(config != nullptr, "Config should not be null");
  REQUIRE(plfft_config_get_data_type(config) == PLFFT_DATA_TYPE_FP32,
          "Default precision should be FP32");
  REQUIRE(plfft_config_get_io_alias(config) == PLFFT_IO_NO_ALIAS,
          "Default IO alias should be NO_ALIAS");
  REQUIRE(plfft_config_get_sme_mode(config) == PLFFT_SME_DISABLED,
          "SME should be disabled by default");
  REQUIRE(plfft_config_get_howmany(config) == 1, "Default howmany should be 1");
  REQUIRE(plfft_config_get_istride(config) == 1, "Default istride should be 1");
  REQUIRE(plfft_config_get_idist(config) == n, "Default idist should be n");
  REQUIRE(plfft_config_get_ostride(config) == 1, "Default ostride should be 1");
  REQUIRE(plfft_config_get_odist(config) == n, "Default odist should be n");
  REQUIRE(plfft_config_get_rigor(config) == PLFFT_MEASURE,
          "Default rigor should be PLFFT_MEASURE");
}

// Verify that configuration setters are reflected by their getters.
void test_config_setters_round_trip() {
  plfft_config_t *config = nullptr;
  plfft_config_create_r2c(&config, 16);
  config_delete_defer config_deleter{config};
  REQUIRE(config != nullptr, "Config should not be null");

  plfft_config_set_data_type(config, PLFFT_DATA_TYPE_FP64);
  plfft_config_set_io_alias(config, PLFFT_IO_MAY_ALIAS);
  plfft_config_set_sme_mode(config, PLFFT_SME_ENABLED);
  plfft_config_set_batch(config, 3, 2, 32, 4, 64);
  plfft_config_set_rigor(config, PLFFT_EXHAUSTIVE);

  REQUIRE(plfft_config_get_data_type(config) == PLFFT_DATA_TYPE_FP64,
          "Precision setter/getter mismatch");
  REQUIRE(plfft_config_get_io_alias(config) == PLFFT_IO_MAY_ALIAS,
          "IO alias setter/getter mismatch");
  REQUIRE(plfft_config_get_sme_mode(config) == PLFFT_SME_ENABLED,
          "SME mode setter/getter mismatch");
  REQUIRE(plfft_config_get_howmany(config) == 3,
          "Howmany setter/getter mismatch");
  REQUIRE(plfft_config_get_istride(config) == 2,
          "Istride setter/getter mismatch");
  REQUIRE(plfft_config_get_idist(config) == 32, "Idist setter/getter mismatch");
  REQUIRE(plfft_config_get_ostride(config) == 4,
          "Ostride setter/getter mismatch");
  REQUIRE(plfft_config_get_odist(config) == 64, "Odist setter/getter mismatch");
  REQUIRE(plfft_config_get_rigor(config) == PLFFT_EXHAUSTIVE,
          "Rigor setter/getter mismatch");
}

// Verify that a plan is independent of its source configuration.
void test_plan_snapshots_configuration() {
  constexpr size_t n = 4;
  complex_plfft<float> input[n] = {
      make_complex<float>(1.0, 0.0), make_complex<float>(2.0, 0.0),
      make_complex<float>(3.0, 0.0), make_complex<float>(4.0, 0.0)};
  complex_plfft<float> output[n] = {};
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  {
    REQUIRE(plfft_config_create(&config, n, PLFFT_FORWARD) == PLFFT_OK,
            "Error creating config for snapshot test");
    config_delete_defer config_deleter{config};
    REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_OK,
            "Error creating plan for snapshot test");

    // A plan must not retain or consult the configuration used to create it.
    plfft_config_set_data_type(config, PLFFT_DATA_TYPE_FP64);
    plfft_config_set_batch(config, 2, 2, 1, 2, 1);
  }
  plan_delete_defer plan_deleter{plan};

  plfft_plan_execute(plan, input, output);
  REQUIRE(output[0].real == 10.0f && output[0].imag == 0.0f,
          "Plan should be independent of its source configuration");
}

// Verify batched in-place R2C/C2R round trips for even and odd sizes.
void test_in_place_real_complex_round_trip() {
  constexpr size_t howmany = 2;
  constexpr size_t batch_real_span = 6;
  constexpr size_t batch_complex_span = 3;

  for (const size_t n : {size_t{4}, size_t{5}}) {
    std::array<float, howmany * batch_real_span> data{};
    std::array<float, howmany * batch_real_span> expected{};
    plfft_config_t *r2c_config = nullptr;
    plfft_config_t *c2r_config = nullptr;
    plfft_plan_t *r2c_plan = nullptr;
    plfft_plan_t *c2r_plan = nullptr;

    for (size_t batch = 0; batch < howmany; ++batch) {
      for (size_t element = 0; element < n; ++element) {
        const size_t index = batch * batch_real_span + element;
        data[index] = static_cast<float>(batch * 10 + element + 1);
        expected[index] = data[index] * static_cast<float>(n);
      }
    }

    REQUIRE(plfft_config_create_r2c(&r2c_config, n) == PLFFT_OK,
            "Error creating in-place R2C config");
    config_delete_defer r2c_config_deleter{r2c_config};
    REQUIRE(plfft_config_create_c2r(&c2r_config, n) == PLFFT_OK,
            "Error creating in-place C2R config");
    config_delete_defer c2r_config_deleter{c2r_config};
    plfft_config_set_io_alias(r2c_config, PLFFT_IO_MAY_ALIAS);
    plfft_config_set_io_alias(c2r_config, PLFFT_IO_MAY_ALIAS);
    plfft_config_set_batch(r2c_config, howmany, 1, batch_real_span, 1,
                           batch_complex_span);
    plfft_config_set_batch(c2r_config, howmany, 1, batch_complex_span, 1,
                           batch_real_span);

    REQUIRE(plfft_plan_create_from_config(&r2c_plan, r2c_config) == PLFFT_OK,
            "Error creating in-place R2C plan");
    plan_delete_defer r2c_plan_deleter{r2c_plan};
    REQUIRE(plfft_plan_create_from_config(&c2r_plan, c2r_config) == PLFFT_OK,
            "Error creating in-place C2R plan");
    plan_delete_defer c2r_plan_deleter{c2r_plan};

    plfft_plan_execute(r2c_plan, data.data(), data.data());
    plfft_plan_execute(c2r_plan, data.data(), data.data());

    for (size_t batch = 0; batch < howmany; ++batch) {
      for (size_t element = 0; element < n; ++element) {
        const size_t index = batch * batch_real_span + element;
        REQUIRE(std::abs(data[index] - expected[index]) < 1.0e-4f,
                "In-place R2C/C2R round trip produced an incorrect value");
      }
    }
  }
}

// Verify exact in-place execution of a real-to-real transform.
void test_in_place_real_to_real_execution() {
  constexpr size_t n = 4;
  float data[n] = {1.0f, 2.0f, 3.0f, 4.0f};
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  REQUIRE(plfft_config_create_r2r(&config, n, PLFFT_R2R_R2HC) == PLFFT_OK,
          "Error creating in-place R2R config");
  config_delete_defer config_deleter{config};
  plfft_config_set_io_alias(config, PLFFT_IO_MAY_ALIAS);
  REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_OK,
          "Error creating in-place R2R plan");
  plan_delete_defer plan_deleter{plan};

  plfft_plan_execute(plan, data, data);
  REQUIRE(data[0] == 10.0f,
          "In-place R2R execution produced an incorrect first output");
}

// Verify batched transforms whose elements are interleaved in memory.
void test_interleaved_batch_layout() {
  constexpr size_t n = 4;
  complex_plfft<float> input[2 * n] = {
      make_complex<float>(1.0, 0.0), make_complex<float>(10.0, 0.0),
      make_complex<float>(2.0, 0.0), make_complex<float>(20.0, 0.0),
      make_complex<float>(3.0, 0.0), make_complex<float>(30.0, 0.0),
      make_complex<float>(4.0, 0.0), make_complex<float>(40.0, 0.0)};
  complex_plfft<float> output[2 * n] = {};
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  REQUIRE(plfft_config_create(&config, n, PLFFT_FORWARD) == PLFFT_OK,
          "Error creating batched config");
  config_delete_defer config_deleter{config};
  plfft_config_set_batch(config, 2, 2, 1, 2, 1);
  REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_OK,
          "Error creating interleaved batch plan");
  plan_delete_defer plan_deleter{plan};

  plfft_plan_execute(plan, input, output);
  REQUIRE(output[0].real == 10.0f && output[0].imag == 0.0f,
          "First interleaved transform has an incorrect first output");
  REQUIRE(output[1].real == 100.0f && output[1].imag == 0.0f,
          "Second interleaved transform has an incorrect first output");
}

// Verify that destroying a null handle is harmless.
void test_destroy_accepts_null() {
  plfft_config_destroy(nullptr);
  plfft_plan_destroy(nullptr);
}

// Verify null configuration and output arguments are rejected.
void test_plan_rejects_invalid_config() {
  plfft_plan_t *plan = nullptr;

  REQUIRE(plfft_config_create(nullptr, 8, PLFFT_FORWARD) == PLFFT_NULL_ARGUMENT,
          "Null config out-parameter should be rejected");
  REQUIRE(plfft_config_create_r2c(nullptr, 8) == PLFFT_NULL_ARGUMENT,
          "Null R2C config out-parameter should be rejected");
  REQUIRE(plfft_config_create_c2r(nullptr, 8) == PLFFT_NULL_ARGUMENT,
          "Null C2R config out-parameter should be rejected");
  REQUIRE(plfft_config_create_r2r(nullptr, 8, PLFFT_R2R_R2HC) ==
              PLFFT_NULL_ARGUMENT,
          "Null R2R config out-parameter should be rejected");

  REQUIRE(plfft_plan_create_from_config(nullptr, nullptr) ==
              PLFFT_NULL_ARGUMENT,
          "Null plan out-parameter should be rejected");

  REQUIRE(plfft_plan_create_from_config(&plan, nullptr) == PLFFT_NULL_ARGUMENT,
          "Null config should be rejected");

  REQUIRE(plan == nullptr, "Failed plan creation should leave plan null");
}

// Verify invalid sizes and batch layouts are rejected during planning.
void test_plan_rejects_invalid_size_and_batch() {
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  {
    plfft_config_create(&config, 0, PLFFT_FORWARD);
    config_delete_defer config_deleter{config};
    REQUIRE(config != nullptr, "Config should not be null");

    REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_INVALID_SIZE,
            "Zero-length transform should be rejected");
    REQUIRE(plan == nullptr, "Invalid size should leave plan null");
  }

  {
    plfft_config_create(&config, -1, PLFFT_FORWARD);
    config_delete_defer config_deleter{config};
    REQUIRE(config != nullptr, "Config should not be null");

    REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_INVALID_SIZE,
            "Negative transform size should be rejected");
    REQUIRE(plan == nullptr, "Invalid size should leave plan null");
  }

  plfft_config_create(&config, 1, PLFFT_FORWARD);
  config_delete_defer config_deleter{config};
  REQUIRE(config != nullptr, "Config should not be null");

  plfft_config_set_batch(config, 0, 1, 1, 1, 1);
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_UNSUPPORTED_BATCH,
          "Zero-length howmany should be rejected");

  plfft_config_set_batch(config, -1, 1, 1, 1, 1);
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_UNSUPPORTED_BATCH,
          "Negative howmany should be rejected");

  plfft_config_set_batch(config, 2, 0, 2, 1, 2);
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_UNSUPPORTED_BATCH,
          "Zero-length input stride should be rejected");

  plfft_config_set_batch(config, 2, 1, 0, 1, 2);
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_UNSUPPORTED_BATCH,
          "Zero-length input distance should be rejected");

  plfft_config_set_batch(config, 2, 1, 1, 0, 2);
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_UNSUPPORTED_BATCH,
          "Zero-length output stride should be rejected");

  plfft_config_set_batch(config, 2, 1, 1, 1, 0);
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_UNSUPPORTED_BATCH,
          "Zero-length output distance should be rejected");

  plfft_config_set_batch(config, 2, 1, 1, 1, 1);
  REQUIRE(plfft_plan_create_from_config(&plan, config) == PLFFT_OK,
          "Valid config should be accepted");
  plan_delete_defer plan_deleter{plan};
}

// Verify that an unknown real-to-real transform kind is rejected.
void test_r2r_rejects_invalid_kind() {
  plfft_config_t *config = nullptr;

  REQUIRE(
      plfft_config_create_r2r(&config, 8, static_cast<plfft_r2r_kind_t>(123)) ==
          PLFFT_INVALID_TRANSFORM_KIND,
      "Invalid R2R kind should be rejected");
  REQUIRE(config == nullptr, "Invalid R2R kind should leave config null");
}

// Verify that real-to-real transforms reject SME mode.
void test_r2r_rejects_sme() {
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  REQUIRE(plfft_config_create_r2r(&config, 8, PLFFT_R2R_R2HC) == PLFFT_OK,
          "Valid R2R config should be created");
  config_delete_defer config_deleter{config};
  plfft_config_set_sme_mode(config, PLFFT_SME_ENABLED);

  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_SME_UNSUPPORTED_R2R,
          "R2R transforms should reject SME mode");
  REQUIRE(plan == nullptr, "Rejected SME R2R plan should remain null");
}

// Verify that fixed-point transforms reject SME mode.
void test_fixed_point_rejects_sme_impl() {
  constexpr std::array data_types{PLFFT_DATA_TYPE_Q7, PLFFT_DATA_TYPE_Q15};

  for (const auto data_type : data_types) {
    plfft_config_t *config = nullptr;
    plfft_plan_t *plan = nullptr;

    REQUIRE(plfft_config_create(&config, 8, PLFFT_FORWARD) == PLFFT_OK,
            "Valid fixed-point config should be created");
    config_delete_defer config_deleter{config};
    plfft_config_set_data_type(config, data_type);
    plfft_config_set_sme_mode(config, PLFFT_SME_ENABLED);

    REQUIRE(plfft_plan_create_from_config(&plan, config) ==
                PLFFT_SME_UNSUPPORTED_FIXED_POINT,
            "Fixed-point transforms should reject SME mode");
    REQUIRE(plan == nullptr,
            "Rejected fixed-point SME plan should remain null");
  }
}

void test_fixed_point_rejects_sme() {
#ifdef PLFFT_ENABLE_FIXED_POINT
  test_fixed_point_rejects_sme_impl();
#endif
}

// Verify that invalid enum values are rejected during creation or planning.
void test_plan_rejects_invalid_enums() {
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  REQUIRE(
      plfft_config_create(&config, 8, static_cast<plfft_direction_t>(123)) ==
          PLFFT_INVALID_TRANSFORM_KIND,
      "Invalid C2C direction should be rejected");
  REQUIRE(config == nullptr, "Invalid C2C direction should leave config null");

  plfft_config_create(&config, 8, PLFFT_FORWARD);
  config_delete_defer config_deleter{config};
  REQUIRE(config != nullptr, "Config should not be null");

  plfft_config_set_io_alias(config, static_cast<plfft_io_alias_t>(123));
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_INVALID_IO_ALIAS,
          "Invalid IO alias should be rejected");

  plfft_config_set_io_alias(config, PLFFT_IO_NO_ALIAS);
  plfft_config_set_sme_mode(config, static_cast<plfft_sme_mode_t>(123));
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_INVALID_SME_MODE,
          "Invalid SME mode should be rejected");

  plfft_config_set_sme_mode(config, PLFFT_SME_DISABLED);
  plfft_config_set_data_type(config, static_cast<plfft_data_type_t>(123));
  REQUIRE(plfft_plan_create_from_config(&plan, config) ==
              PLFFT_INVALID_DATA_TYPE,
          "Invalid data type should be rejected");
}

// Verify that known and unknown statuses always have printable text.
void test_status_strings_are_non_null() {
  REQUIRE(plfft_status_to_string(PLFFT_OK) != nullptr,
          "OK status string should not be null");
  REQUIRE(plfft_status_to_string(PLFFT_INVALID_SIZE) != nullptr,
          "Invalid size status string should not be null");
  REQUIRE(plfft_status_to_string(PLFFT_SME_NOT_ENABLED) != nullptr,
          "SME-not-enabled status string should not be null");
  REQUIRE(plfft_status_to_string(PLFFT_SME_NOT_AVAILABLE) != nullptr,
          "SME-not-available status string should not be null");
  REQUIRE(plfft_status_to_string(PLFFT_SME_UNSUPPORTED_FIXED_POINT) != nullptr,
          "SME-unsupported-fixed-point status string should not be null");
  REQUIRE(plfft_status_to_string(PLFFT_FP16_NOT_AVAILABLE) != nullptr,
          "FP16-not-available status string should not be null");
  REQUIRE(plfft_status_to_string(static_cast<plfft_status_t>(999)) != nullptr,
          "Unknown status string should not be null");
}

int main() {
  test_config_defaults();
  test_config_setters_round_trip();
  test_plan_snapshots_configuration();
  test_in_place_real_complex_round_trip();
  test_in_place_real_to_real_execution();
  test_interleaved_batch_layout();
  test_destroy_accepts_null();
  test_status_strings_are_non_null();

  test_plan_rejects_invalid_config();
  test_plan_rejects_invalid_size_and_batch();
  test_plan_rejects_invalid_enums();
  test_r2r_rejects_invalid_kind();
  test_r2r_rejects_sme();
  test_fixed_point_rejects_sme();

  test_data_type<float>();
  test_data_type<double>();
  test_data_type<__fp16>();
#if PLFFT_ENABLE_FIXED_POINT
  test_data_type<int8_t>();
  test_data_type<int16_t>();
#endif
  return 0;
}
