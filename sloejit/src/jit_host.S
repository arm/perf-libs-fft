/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

.text
.global jit_host
jit_host:
    .rept BM_HOST_INS
    udf #0
    .endr
#ifdef __ELF__
.size  jit_host, .-jit_host
#endif
