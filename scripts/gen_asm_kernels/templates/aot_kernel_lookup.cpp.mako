## SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
##
## SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
<%include file="banner.txt.mako"/>\
#include "${kernel_header_name}"
#include "plfft_attrs.hpp"
#include "kernel_lookup.hpp"

#define ARM_NEW_ZA __arm_new("za") PLFFT_TARGET_SME

using namespace plfft;

#define NUM_FFT_BASE_KERNELS ${table_size}

% for lookup in config:
% for table in lookup.tables:
% if lookup.is_fp16:
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif
% if lookup.is_fixed_point:
#if PLFFT_ENABLE_FIXED_POINT
% endif
% if table.target.name == "sme":
#ifdef PLFFT_ENABLE_SME
% endif
<% za_kernels = [kernel for kernel in table.kernels.values() if kernel.uses_za] %>\
% if za_kernels:
namespace {
% for kernel in za_kernels:
ARM_NEW_ZA void
${kernel.name}_za_wrap(${lookup.wrapper_params}) {
  __asm__ volatile("" ::: "za");
  ${kernel.name}(${lookup.wrapper_args});
}
% endfor
} // namespace
% endif
static
${table.fft_func_t} *
${table.name}[NUM_FFT_BASE_KERNELS] = {
% for n in range(table_size):
% if n in table.kernels:
% if table.kernels[n].uses_za:
  ${table.kernels[n].name}_za_wrap,
% else:
  ${table.kernels[n].name},
% endif
% else:
  nullptr,
% endif
% endfor
};
% if table.target.name == "sme":
#endif // PLFFT_ENABLE_SME
% endif
% if lookup.is_fixed_point:
#endif // PLFFT_ENABLE_FIXED_POINT
% endif
% if lookup.is_fp16:
#endif // __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif

% endfor
% endfor

namespace plfft {
% for lookup in config:
% if lookup.is_fp16:
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif
% if lookup.is_fixed_point:
#if PLFFT_ENABLE_FIXED_POINT
% endif
template<>
${lookup.fft_func_t} *
${lookup.name}(int64_t n, plfft_direction_t dir, bool want_sve, bool want_sme) {
  if (n < 0 || NUM_FFT_BASE_KERNELS <= n) {
    return nullptr;
  }

<% sme_tables = lookup.tables_for_targets(sme) %>
## ASIMDHP is a separate KernelTarget from neon, but runtime dispatch is shared
<% neon_tables = lookup.tables_for_targets(neon, asimdhp) %>
<% sve_tables = lookup.tables_for_targets(sve) %>
% if sme_tables:
#ifdef PLFFT_ENABLE_SME
  if (want_sme) {
% if len(sme_tables) == 1:
    auto *sme_fn = ${sme_tables[0].name}[n];
    if (sme_fn != nullptr) {
      return sme_fn;
    }
% else:
    ${lookup.fft_func_t} *sme_fn = nullptr;
    switch (dir) {
% for table in sme_tables:
    case ${table.dir.plfft_value}: sme_fn = ${table.name}[n]; break;
% endfor
    }
    if (sme_fn != nullptr) {
      return sme_fn;
    }
% endif
  }
#endif // PLFFT_ENABLE_SME
% endif
% if sve_tables:
% if len(sve_tables) == 1:
  if (want_sve) {
    return ${sve_tables[0].name}[n];
  }
% else:
  if (want_sve) {
    switch (dir) {
% for table in sve_tables:
    case ${table.dir.plfft_value}: return ${table.name}[n];
% endfor
    }
  }
% endif
% if len(neon_tables) == 0:
  return nullptr;
% elif len(neon_tables) == 1:
  return ${neon_tables[0].name}[n];
% else:
  switch (dir) {
% for table in neon_tables:
    case ${table.dir.plfft_value}: return ${table.name}[n];
% endfor
  }
% endif
% else:
% if len(neon_tables) == 0:
  return nullptr;
% elif len(neon_tables) == 1:
  return ${neon_tables[0].name}[n];
% else:
  switch (dir) {
% for table in neon_tables:
  case ${table.dir.plfft_value}: return ${table.name}[n];
% endfor
  }
% endif
% endif

% if neon_tables:
  assert(false && "unreachable");
% endif
  return nullptr;
}
% if lookup.is_fixed_point:
#endif // PLFFT_ENABLE_FIXED_POINT
% endif
% if lookup.is_fp16:
#endif // __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif

% endfor
} // namespace plfft
