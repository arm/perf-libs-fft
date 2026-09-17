## SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
##
## SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
<%include file="banner.txt.mako"/>\
#include "kernel_data.hpp"

using namespace plfft;

extern "C" {
% for lookup in config:
% for table in lookup.tables:
% if lookup.is_fixed_point:
#if PLFFT_ENABLE_FIXED_POINT
% endif
% if table.target.name == "sme":
#if PLFFT_ENABLE_SME
% endif
% for kernel in table.kernels.values():
${kernel.fft_func_t} ${kernel.name};
% endfor
% if table.target.name == "sme":
#endif // PLFFT_ENABLE_SME
% endif
% if lookup.is_fixed_point:
#endif // PLFFT_ENABLE_FIXED_POINT
% endif
% endfor
% endfor
}
