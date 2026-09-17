/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  enum { plfft_example_n = 8 };

  const size_t n = plfft_example_n;
  float input[plfft_example_n];
  float output[plfft_example_n];
  plfft_config_t *config = NULL;
  plfft_plan_t *plan = NULL;
  plfft_status_t status = PLFFT_OK;
  bool validation_passed = false;
  float expected_first = 0.0f;

  for (size_t i = 0; i < n; ++i) {
    input[i] = (float)(i + 1);
    output[i] = 0.0f;
    expected_first += input[i];
  }

  status = plfft_config_create_r2r(&config, n, PLFFT_R2R_R2HC);
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

  printf("R2R R2HC transform, n=%zu\n", n);
  for (size_t i = 0; i < n; ++i) {
    printf("out[%zu] = %+8.4f\n", i, output[i]);
  }

  validation_passed = (output[0] == expected_first);
  if (!validation_passed) {
    printf("unexpected first output\n");
    goto done;
  }

  printf("first output matches the input sum: %.4f\n", expected_first);

done:
  plfft_plan_destroy(plan);
  plfft_config_destroy(config);
  return status == PLFFT_OK && validation_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
