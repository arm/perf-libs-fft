# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

from dataclasses import dataclass


@dataclass(frozen=True)
class KernelTarget:
    """A target under which an assembly kernel is generated and built."""

    # Capabilities for each name are defined in parse.cpp:parse_target
    # - add a new entry there if adding a new KernelTarget
    name: str
    # -march flag gets generated into kernels.cmake
    march: str


NEON_TARGET = KernelTarget("neon", "armv8-a")
ASIMDHP_TARGET = KernelTarget("asimdhp", "armv8-a+fp16")
SVE_TARGET = KernelTarget("sve", "armv8-a+sve")
SME_TARGET = KernelTarget("sme", "armv9-a+sme")

KERNEL_TARGETS = (NEON_TARGET, ASIMDHP_TARGET, SVE_TARGET, SME_TARGET)
