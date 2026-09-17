# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class BenchmarkCase:
    n: int
    transform_kind: str
    datatype: str
    direction: str
    io_alias: str
    howmany: int
    istride: int
    idist: int
    ostride: int
    odist: int
    niters: int
    warmup_ms: int
    ninvocs: int
    sme: bool
    rigor: str


@dataclass(frozen=True)
class BenchmarkResult(BenchmarkCase):
    value: float
    unit: str


class AbstractRunner(ABC):
    @classmethod
    @abstractmethod
    def create(
        cls, bench_binary: Path, verbose: bool, arg: str | None
    ) -> "AbstractRunner":
        pass

    @abstractmethod
    def execute(self, case: BenchmarkCase) -> BenchmarkResult:
        pass
