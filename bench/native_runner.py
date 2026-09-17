# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import json
import shlex
import subprocess
import sys

from pathlib import Path

from typing_extensions import Self

from bench.benchmark_types import (
    AbstractRunner,
    BenchmarkCase,
    BenchmarkResult,
)


def bench_command(bench_binary: Path, case: BenchmarkCase) -> list[str]:
    cmd = [
        str(bench_binary),
        "--n",
        str(case.n),
        "--transform-kind",
        case.transform_kind,
        "--data-type",
        case.datatype,
        "--direction",
        case.direction,
        "--io-alias",
        case.io_alias,
        "--howmany",
        str(case.howmany),
        "--istride",
        str(case.istride),
        "--idist",
        str(case.idist),
        "--ostride",
        str(case.ostride),
        "--odist",
        str(case.odist),
        "--niters",
        str(case.niters),
        "--warmup-ms",
        str(case.warmup_ms),
        "--rigor",
        str(case.rigor),
    ]

    if case.sme:
        cmd.append("--sme")

    return cmd


def parse_result(case: BenchmarkCase, bench_output: str) -> BenchmarkResult:
    data = json.loads(bench_output)
    return BenchmarkResult(**vars(case), value=float(data["value"]), unit=data["unit"])


class NativeRunner(AbstractRunner):
    def __init__(self, bench_binary: Path, verbose: bool, disable_aslr: bool):
        self.bench_binary = bench_binary
        self.verbose = verbose
        if disable_aslr and sys.platform != "linux":
            raise ValueError("Can only disable ASLR on Linux")
        self.disable_aslr = disable_aslr

    @classmethod
    def create(cls, bench_binary: Path, verbose: bool, arg: str | None = None) -> Self:
        if arg not in (None, "disable-aslr"):
            raise ValueError(f"unsupported native runner argument: {arg}")
        return cls(bench_binary, verbose, disable_aslr=arg == "disable-aslr")

    def execute(self, case: BenchmarkCase) -> BenchmarkResult:
        prefix = ["setarch", "-R"] if self.disable_aslr else []
        cmd = [*prefix, *bench_command(self.bench_binary, case)]
        if self.verbose:
            print("+ " + shlex.join(cmd))
        completed = subprocess.run(cmd, check=True, text=True, capture_output=True)
        return parse_result(case, completed.stdout)
