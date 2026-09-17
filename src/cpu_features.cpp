/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "cpu_features.hpp"
#include "plfft_util.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <sys/auxv.h>
#endif

namespace plfft {
namespace {

bool disabled(const char *name) {
  const char *value = std::getenv(name);
  return value != nullptr && std::strcmp(value, "1") == 0;
}

#if defined(__linux__) || defined(__ANDROID__)
cpu_features features_from_string(const char *features) {
  cpu_features result;
  while (*features != '\0') {
    features += std::strspn(features, " \t\n");
    const auto length = std::strcspn(features, " \t\n");
    if (length == 7 && std::strncmp(features, "asimdhp", length) == 0) {
      result.asimdhp = true;
    } else if (length == 4 && std::strncmp(features, "fcma", length) == 0) {
      result.fcma = true;
    } else if (length == 3 && std::strncmp(features, "sve", length) == 0) {
      result.sve = true;
    } else if (length == 3 && std::strncmp(features, "sme", length) == 0) {
      result.sme = true;
    } else if (length == 4 && std::strncmp(features, "sme2", length) == 0) {
      result.sme2 = true;
    }
    if (length == 0) {
      break;
    }
    features += length;
  }
  return result;
}

cpu_features features_from_hwcap(unsigned long hwcap, unsigned long hwcap2) {
  return {
      .asimdhp = (hwcap & (1UL << 10)) != 0,
      .fcma = (hwcap & (1UL << 14)) != 0,
      .sve = (hwcap & (1UL << 22)) != 0,
      .sme = (hwcap2 & (1UL << 23)) != 0,
      .sme2 = (hwcap2 & (1UL << 37)) != 0,
  };
}

cpu_features features_from_proc_cpuinfo() {
  if (auto *input = std::fopen("/proc/cpuinfo", "r")) {
    char line[4096];
    while (std::fgets(line, sizeof(line), input) != nullptr) {
      if (std::strncmp(line, "Features", 8) == 0) {
        const auto colon = std::strchr(line, ':');
        const auto result =
            colon == nullptr ? cpu_features{} : features_from_string(colon + 1);
        std::fclose(input);
        return result;
      }
    }
    std::fclose(input);
  }
  return {};
}
#endif

#if defined(__APPLE__)
bool sysctl_feature(const char *name) {
  int value = 0;
  std::size_t size = sizeof(value);
  return sysctlbyname(name, &value, &size, nullptr, 0) == 0 && value == 1;
}
#endif

cpu_features detect_cpu_features() {
#if defined(__linux__) || defined(__ANDROID__)
  if (const auto hwcap = getauxval(AT_HWCAP)) {
    return features_from_hwcap(hwcap, getauxval(AT_HWCAP2));
  }
  return features_from_proc_cpuinfo();
#elif defined(__APPLE__)
  return {
      .asimdhp = sysctl_feature("hw.optional.arm.FEAT_FP16"),
      .fcma = sysctl_feature("hw.optional.arm.FEAT_FCMA"),
      .sve = false,
      .sme = sysctl_feature("hw.optional.arm.FEAT_SME"),
      .sme2 = sysctl_feature("hw.optional.arm.FEAT_SME2"),
  };
#else
  // If we can't work out what is available, disable everything by default
  return cpu_features{};
#endif
}

} // namespace

cpu_features get_cpu_features() {
  THREAD_LOCAL const auto features = [] {
    auto result = detect_cpu_features();

    // Private controls for testing and experimentation
    if (disabled("PLFFT_DISABLE_FP16")) {
      result.asimdhp = false;
    }
    if (disabled("PLFFT_DISABLE_FCMA")) {
      result.fcma = false;
    }
    if (disabled("PLFFT_DISABLE_SVE")) {
      result.sve = false;
    }
    if (disabled("PLFFT_DISABLE_SME")) {
      result.sme = false;
    }
    if (disabled("PLFFT_DISABLE_SME2")) {
      result.sme2 = false;
    }
    return result;
  }();
  return features;
}

} // namespace plfft
