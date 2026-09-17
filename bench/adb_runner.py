# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import shlex
import subprocess
import warnings

from pathlib import Path

from typing_extensions import Self

from bench.benchmark_types import (
    AbstractRunner,
    BenchmarkCase,
    BenchmarkResult,
)
from bench.native_runner import bench_command, parse_result

DEFAULT_DEVICE_DIR = Path("/data/local/tmp")
DEVICE_BENCH_NAME = "plfft_bench"


def check_adb_connection() -> None:
    completed = subprocess.run(
        ["adb", "get-state"], check=False, text=True, capture_output=True
    )

    if completed.returncode != 0 or completed.stdout.strip() != "device":
        raise RuntimeError("no active ADB connection")


def parse_sha256sum(output: str) -> str | None:
    fields = output.split()
    if len(fields) < 2:
        return None
    return fields[0]


def local_sha256(path: Path) -> str | None:
    try:
        completed = subprocess.run(
            ["sha256sum", str(path)], check=False, text=True, capture_output=True
        )
    except FileNotFoundError:
        warnings.warn("sha256sum was not found; pushing benchmark binary", stacklevel=2)
        return None

    if completed.returncode != 0:
        warnings.warn("sha256sum failed; pushing benchmark binary", stacklevel=2)
        return None

    return parse_sha256sum(completed.stdout)


def remote_sha256(path: Path) -> str | None:
    completed = subprocess.run(
        ["adb", "shell", "sha256sum", shlex.quote(str(path))],
        check=False,
        text=True,
        capture_output=True,
    )
    if completed.returncode != 0:
        return None

    return parse_sha256sum(completed.stdout)


class AdbRunner(AbstractRunner):
    def __init__(self, bench_binary: Path, verbose: bool, device_dir_arg: str | None):
        self.bench_binary = bench_binary
        self.verbose = verbose
        device_dir = (
            Path(device_dir_arg) if device_dir_arg is not None else DEFAULT_DEVICE_DIR
        )
        self.device_bench = device_dir / DEVICE_BENCH_NAME

        check_adb_connection()
        local_hash = local_sha256(self.bench_binary)
        remote_hash = remote_sha256(self.device_bench)

        # To save time pushing the bench binary, check whether the
        # local and remote versions match and skip copying if so. If
        # either device does not have sha256sum do the push -
        # local_sha256 will issue a warning for this but assume we do
        # not have permission to install remotely
        if local_hash is None or local_hash != remote_hash:
            push_cmd = [
                "adb",
                "push",
                str(self.bench_binary),
                str(self.device_bench),
            ]
            if self.verbose:
                print("+ " + shlex.join(push_cmd))
            subprocess.run(push_cmd, check=True)

        chmod_cmd = ["adb", "shell", "chmod", "+x", shlex.quote(str(self.device_bench))]
        if self.verbose:
            print("+ " + shlex.join(chmod_cmd))
        subprocess.run(chmod_cmd, check=True)

    @classmethod
    def create(cls, bench_binary: Path, verbose: bool, device_dir: str | None) -> Self:
        return cls(bench_binary, verbose, device_dir)

    def execute(self, case: BenchmarkCase) -> BenchmarkResult:
        bench_cmd = bench_command(self.device_bench, case)
        cmd = ["adb", "shell", *map(shlex.quote, bench_cmd)]

        if self.verbose:
            print("+ " + shlex.join(cmd))
        completed = subprocess.run(cmd, check=True, text=True, capture_output=True)
        res: BenchmarkResult = parse_result(case, completed.stdout)
        return res
