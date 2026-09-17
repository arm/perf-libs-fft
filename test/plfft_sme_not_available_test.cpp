/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "plfft.h"
#include "test_utils.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
  plfft_config_t *config = nullptr;
  plfft_plan_t *plan = nullptr;

  REQUIRE(plfft_config_create(&config, 8, PLFFT_FORWARD) == PLFFT_OK,
          "Failed to create the SME test configuration");

  plfft_config_set_sme_mode(config, PLFFT_SME_ENABLED);
  const plfft_status_t status = plfft_plan_create_from_config(&plan, config);

  plfft_plan_destroy(plan);
  plfft_config_destroy(config);

  REQUIRE(getenv("PLFFT_DISABLE_SME"),
          "Expected PLFFT_DISABLE_SME environment variable to be set");

  REQUIRE(status == PLFFT_SME_NOT_AVAILABLE,
          "Expected PLFFT_SME_NOT_AVAILABLE");
  return EXIT_SUCCESS;
}
