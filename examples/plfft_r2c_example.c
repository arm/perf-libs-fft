/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct complex_fp32_t {
  float real;
  float imag;
} complex_fp32_t;

int main() {
  enum { plfft_example_n = 8 };

  const size_t n = plfft_example_n;
  float input[plfft_example_n];
  complex_fp32_t output[plfft_example_n / 2 + 1];
  plfft_config_t *config = NULL;
  plfft_plan_t *plan = NULL;
  plfft_status_t status = PLFFT_OK;
  bool validation_passed = false;
  float expected_dc = 0.0f;

  for (size_t i = 0; i < n; ++i) {
    input[i] = (float)(i + 1);
    expected_dc += input[i];
  }
  for (size_t i = 0; i < n / 2 + 1; ++i) {
    output[i].real = 0.0f;
    output[i].imag = 0.0f;
  }

  status = plfft_config_create_r2c(&config, n);
  if (status != PLFFT_OK) {
    printf("config creation failed: %s\n", plfft_status_to_string(status));
    goto done;
  }

  status = plfft_plan_create_from_config(&plan, config);
  if (status != PLFFT_OK) {
    printf("plan creation failed: %s\n", plfft_status_to_string(status));
    goto done;
  }

  plfft_plan_execute(plan, input, output);

  printf("forward R2C FFT, n=%zu\n", n);
  for (size_t i = 0; i < n / 2 + 1; ++i) {
    printf("out[%zu] = %+8.4f %+8.4fi\n", i, output[i].real, output[i].imag);
  }

  validation_passed = (output[0].real == expected_dc && output[0].imag == 0);
  if (!validation_passed) {
    printf("unexpected first output\n");
    goto done;
  }

  printf("First output matches the input sum: %.4f\n", expected_dc);

done:
  plfft_plan_destroy(plan);
  plfft_config_destroy(config);
  return status == PLFFT_OK && validation_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
