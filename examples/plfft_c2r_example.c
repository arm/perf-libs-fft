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
  complex_fp32_t input[plfft_example_n / 2 + 1] = {
      {1.0f, 0.0f}, {2.0f, 10.0f}, {3.0f, -4.0f}, {4.0f, 7.0f}, {5.0f, 0.0f},
  };
  float output[plfft_example_n];
  plfft_config_t *config = NULL;
  plfft_plan_t *plan = NULL;
  plfft_status_t status = PLFFT_OK;
  bool validation_passed = false;
  float expected_first = input[0].real + input[plfft_example_n / 2].real;

  for (size_t i = 1; i < n / 2; ++i) {
    expected_first += 2.0f * input[i].real;
  }
  for (size_t i = 0; i < n; ++i) {
    output[i] = 0.0f;
  }

  status = plfft_config_create_c2r(&config, n);
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

  printf("backward C2R FFT, n=%zu\n", n);
  for (size_t i = 0; i < n; ++i) {
    printf("out[%zu] = %+8.4f\n", i, output[i]);
  }

  validation_passed = (output[0] == expected_first);
  if (!validation_passed) {
    printf("unexpected first output\n");
    goto done;
  }

  printf("first output matches the Hermitian input sum: %.4f\n",
         expected_first);

done:
  plfft_plan_destroy(plan);
  plfft_config_destroy(config);
  return status == PLFFT_OK && validation_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
