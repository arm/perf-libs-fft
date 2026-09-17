## SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
##
## SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
<%include file="banner.txt.mako"/>\
% if lookup.is_fp16:
#ifdef __ARM_FEATURE_FP16_VECTOR_ARITHMETIC

% endif
#include "asm_macro_defs.h"

% for kernel in kernels:
${kernel}
% endfor
% if lookup.is_fp16:
#endif // __ARM_FEATURE_FP16_VECTOR_ARITHMETIC
% endif
