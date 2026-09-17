#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import argparse
import importlib.util
import itertools
import json
import sys

from collections.abc import Iterable, Sequence
from dataclasses import fields
from pathlib import Path
from types import ModuleType
from typing import TextIO

if __name__ == "__main__" and __package__ is None:
    print(  # type: ignore[unreachable]
        "error: this version of bench_driver.py cannot be run as a standalone script. \n"
        "       Either invoke it as a module using\n"
        "         python -m bench.bench_driver ...\n"
        "       or use the version generated in the build tree:\n"
        "         <build_dir>/bench_driver.py ...",
        file=sys.stderr,
    )
    sys.exit(1)

from bench.adb_runner import AdbRunner
from bench.benchmark_types import (
    AbstractRunner,
    BenchmarkCase,
    BenchmarkResult,
)
from bench.native_runner import NativeRunner

RunnerClass = type[AbstractRunner]

RUNNERS: dict[str, RunnerClass] = {"native": NativeRunner, "adb": AdbRunner}


def register_runner(name: str, runner: RunnerClass) -> None:
    if name in RUNNERS:
        raise ValueError(f"Runner name '{name}' is already registered")
    RUNNERS[name] = runner


def load_runner_hook(path: Path) -> None:
    spec = importlib.util.spec_from_file_location("bench_runner_hook", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load runner hook: {path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    register_hook_module(path, module)


def register_hook_module(path: Path, module: ModuleType) -> None:
    register = getattr(module, "register", None)
    if register is None:
        raise RuntimeError(f"Runner hook {path} does not define register()")
    register(register_runner)


def load_default_runner_hook(hook_dir: Path) -> None:
    hook = hook_dir / "bench_runner_hook.py"
    if hook.exists():
        load_runner_hook(hook)


def split_runner_arg(runner_arg: str) -> tuple[str, str | None]:
    name, separator, arg = runner_arg.partition(":")
    return name, (arg if separator else None)


def run_cases(
    cases: Iterable[BenchmarkCase],
    runner: AbstractRunner,
) -> list[BenchmarkResult]:
    return [
        min((runner.execute(case) for i in range(case.ninvocs)), key=lambda c: c.value)
        for case in cases
    ]


def logical_lengths(transform_kind: str, n: int) -> tuple[int, int]:
    hermitian_n = n // 2 + 1
    if transform_kind == "c2c":
        return n, n
    if transform_kind == "r2c":
        return n, hermitian_n
    if transform_kind == "c2r":
        return hermitian_n, n
    raise ValueError(f"unknown transform kind '{transform_kind}'")


def resolve_layout(
    layout: str, input_n: int, output_n: int, howmany: int
) -> tuple[int, int, int, int]:
    if layout == "uu":
        return howmany, 1, howmany, 1
    if layout == "tu":
        return 1, input_n, howmany, 1
    if layout == "ut":
        return howmany, 1, 1, output_n
    if layout == "tt":
        return 1, input_n, 1, output_n
    raise ValueError(f"unknown layout '{layout}'")


def stride_dist_cases(
    args: argparse.Namespace,
    transform_kind: str,
    n: int,
    howmany: int,
) -> list[tuple[int, int, int, int]]:
    input_n, output_n = logical_lengths(transform_kind, n)
    if args.layout is not None:
        return [resolve_layout(args.layout, input_n, output_n, howmany)]

    istrides = args.istride or [1]
    ostrides = args.ostride or [1]
    cases = []
    for istride, ostride in itertools.product(istrides, ostrides):
        default_idist = input_n * istride
        default_odist = output_n * ostride
        idists = args.idist or [default_idist]
        odists = args.odist or [default_odist]
        for idist, odist in itertools.product(idists, odists):
            cases.append((istride, idist, ostride, odist))
    return cases


def cases_from_args(args: argparse.Namespace) -> list[BenchmarkCase]:
    if args.layout and any(
        value is not None
        for value in (args.istride, args.idist, args.ostride, args.odist)
    ):
        raise ValueError("layout cannot be combined with explicit stride/dist options")

    for long_name, short_name in [
        ("exhaustive", "x"),
        ("patient", "p"),
        ("measure", "m"),
        ("estimate", "s"),
    ]:
        if long_name in args.rigor and short_name in args.rigor:
            raise ValueError(
                f"{short_name} is the short name for {long_name}. Please use only one."
            )

    cases = []
    for n, transform_kind, datatype, io_alias, howmany, rigor in itertools.product(
        args.n,
        args.transform_kind,
        args.datatype,
        args.io_alias,
        args.howmany,
        args.rigor,
    ):
        if transform_kind == "c2c":
            directions = args.direction or ["forward"]
        else:
            directions = ["backward" if transform_kind == "c2r" else "forward"]
        for direction in directions:
            for istride, idist, ostride, odist in stride_dist_cases(
                args, transform_kind, n, howmany
            ):
                cases.append(
                    BenchmarkCase(
                        n=n,
                        transform_kind=transform_kind,
                        datatype=datatype,
                        direction=direction,
                        io_alias=io_alias,
                        howmany=howmany,
                        istride=istride,
                        idist=idist,
                        ostride=ostride,
                        odist=odist,
                        niters=args.niters,
                        warmup_ms=args.warmup_ms,
                        ninvocs=args.ninvocs,
                        sme=args.sme,
                        rigor=rigor,
                    )
                )
    return cases


def make_parser(default_bench_binary: Path) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run PLFFT benchmark cases and aggregate the results.",
        allow_abbrev=False,
    )
    parser.add_argument(
        "-k",
        "--transform-kind",
        nargs="+",
        choices=["c2c", "r2c", "c2r"],
        default=["c2c"],
        help="transform kinds (default: c2c)",
    )
    parser.add_argument(
        "bench_binary",
        nargs="?",
        type=Path,
        default=default_bench_binary,
        help=f"benchmark binary to run (default: {default_bench_binary})",
    )
    parser.add_argument(
        "-n", "--n", required=True, nargs="+", type=int, help="transform sizes"
    )
    parser.add_argument(
        "-p",
        "--datatype",
        nargs="+",
        default=["f32"],
        help="datatypes (default: f32)",
    )
    parser.add_argument(
        "-d",
        "--direction",
        nargs="+",
        help="C2C directions (default: forward; inferred for real transforms)",
    )
    parser.add_argument(
        "-a",
        "--io-alias",
        nargs="+",
        default=["no"],
        help="alias modes (default: no)",
    )
    parser.add_argument(
        "-H",
        "--howmany",
        nargs="+",
        type=int,
        default=[1],
        help="batch counts (default: 1)",
    )
    parser.add_argument(
        "-l",
        "--layout",
        choices=["uu", "tu", "ut", "tt"],
        help="single layout shorthand",
    )
    parser.add_argument(
        "--is", dest="istride", nargs="+", type=int, help="input strides"
    )
    parser.add_argument(
        "--id", dest="idist", nargs="+", type=int, help="input distances"
    )
    parser.add_argument(
        "--os", dest="ostride", nargs="+", type=int, help="output strides"
    )
    parser.add_argument(
        "--od", dest="odist", nargs="+", type=int, help="output distances"
    )
    parser.add_argument(
        "--niters",
        type=int,
        default=10,
        help="number of timed executions per case (default: 10)",
    )
    parser.add_argument(
        "--warmup-ms",
        type=int,
        default=50,
        help="warm-up duration per case in milliseconds (default: 50)",
    )
    parser.add_argument(
        "--ninvocs",
        type=int,
        default=1,
        help="number of independent benchmark invocations per case - report best (default: 1)",
    )
    parser.add_argument(
        "--sme",
        action="store_true",
        help="allow SME kernels. applies to entire batch of cases rather than participating in the cross-product",
    )
    parser.add_argument(
        "--runner",
        help="registered runner to use, optionally as runner:config",
    )
    parser.add_argument(
        "--runner-hook",
        action="append",
        default=[],
        type=Path,
        help="extra runner hook to load after bench_runner_hook.py",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="print benchmark invocations to stdout before running them",
    )
    parser.add_argument(
        "-t",
        "--table",
        action="store_true",
        help="print results as a plain-text table instead of JSON",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="write results to this file instead of stdout",
    )
    parser.add_argument(
        "-r",
        "--rigor",
        nargs="+",
        choices=["estimate", "s", "measure", "m", "patient", "p", "exhaustive", "x"],
        default=["estimate"],
        help="planning rigor (default: estimate)",
    )
    if sys.platform == "linux":
        parser.add_argument(
            "--allow-aslr",
            action="store_true",
            help="without this flag benchmarks run using the native runner "
            "are invoked with setarch -R to disable Address Space Layout "
            "Randomization. --allow-aslr suppresses this",
        )
    return parser


def result_columns(results: Sequence[BenchmarkResult]) -> list[str]:
    def constant_column(col: str) -> bool:
        return len({getattr(result, col) for result in results}) == 1

    cols = [field.name for field in fields(BenchmarkResult)]
    # Strip out unit and value in case they get removed by the
    # constant_column check - then add them back after
    cols.remove("unit")
    cols.remove("value")
    # Strip out columns which didn't change for more readable output
    cols = [col for col in cols if not constant_column(col)]
    return cols + ["unit", "value"]


def print_results(results: Sequence[BenchmarkResult], out: TextIO) -> None:
    columns = result_columns(results)

    def cell_text(name: str, value: object) -> str:
        if name == "value":
            return f"{value:.2e}"
        return str(value)

    print("\t".join(columns), file=out)
    for result in results:
        print(
            "\t".join(cell_text(column, getattr(result, column)) for column in columns),
            file=out,
        )


def print_json_results(results: Sequence[BenchmarkResult], out: TextIO) -> None:
    rows = [vars(result) for result in results]
    print(json.dumps(rows, indent=2), file=out)


def print_output(
    results: Sequence[BenchmarkResult], *, table: bool, out: TextIO
) -> None:
    if table:
        print_results(results, out)
    else:
        print_json_results(results, out)


def main(
    *,
    default_bench_binary: Path,
    default_runner_hook_dir: Path,
) -> int:
    parser = make_parser(default_bench_binary)
    args = parser.parse_args()

    load_default_runner_hook(default_runner_hook_dir)
    for hook in args.runner_hook:
        load_runner_hook(hook)

    runner_arg = args.runner or "native"
    runner_name, runner_config = split_runner_arg(runner_arg)
    if runner_name not in RUNNERS:
        raise RuntimeError(
            f"Runner '{runner_arg}' not registered. Available runners are {list(RUNNERS.keys())}"
        )
    bench_binary = args.bench_binary.resolve(strict=True)

    if sys.platform == "linux" and args.allow_aslr and runner_name != "native":
        raise RuntimeError("Cannot enable ASLR for non-native runner")
    if sys.platform == "linux" and runner_name == "native":
        assert runner_config is None
        runner_config = "disable-aslr" if not args.allow_aslr else None

    runner = RUNNERS[runner_name].create(bench_binary, args.verbose, runner_config)

    cases = cases_from_args(args)
    results = run_cases(
        cases,
        runner,
    )
    if args.output is not None:
        with args.output.open("w") as out:
            print_output(results, table=args.table, out=out)
    else:
        print_output(results, table=args.table, out=sys.stdout)
    return 0


if __name__ == "__main__":
    driver_dir = Path(__file__).resolve().parent
    sys.exit(
        main(
            default_bench_binary=driver_dir / "bench",
            default_runner_hook_dir=driver_dir,
        )
    )
