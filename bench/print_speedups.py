#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import argparse
import json
import statistics
import sys

from pathlib import Path
from typing import cast

FieldValue = bool | float | int | str
Result = dict[str, FieldValue]
CaseKey = tuple[tuple[str, FieldValue], ...]


def remove_constant_fields(results: list[Result]) -> None:
    constant_fields = []
    for key in {key for result in results for key in result} - {"value"}:
        values = {result.get(key) for result in results}
        if len(values) == 1:
            constant_fields.append(key)

    for result in results:
        for key in constant_fields:
            del result[key]


def case_key(result: Result) -> CaseKey:
    return tuple(
        (name, value) for name, value in sorted(result.items()) if name != "value"
    )


def case_name(case: CaseKey, value_widths: dict[str, int] | None = None) -> str:
    if value_widths is None:
        return " ".join(f"{name}={value}" for name, value in case)
    return " ".join(
        f"{name}={str(value):<{value_widths[name]}}" for name, value in case
    )


def case_sort_key(
    case: CaseKey,
) -> tuple[tuple[str, tuple[str, float | int | str]], ...]:
    # Sort numeric fields numerically, lexically otherwise
    return tuple(
        (
            name,
            (
                ("number", value)
                if isinstance(value, (int, float)) and not isinstance(value, bool)
                else (type(value).__name__, str(value))
            ),
        )
        for name, value in case
    )


def get_data(data: list[Result]) -> dict[CaseKey, float]:
    return {case_key(result): cast(float, result["value"]) for result in data}


def print_speedups(
    old_filename: Path, new_filename: Path, include_absolute: bool = False
) -> None:
    old_results = json.loads(old_filename.read_text())
    new_results = json.loads(new_filename.read_text())

    # Removing on result of a concat is OK because the lists contain
    # references to dicts
    remove_constant_fields(old_results + new_results)
    old_values = get_data(old_results)
    new_values = get_data(new_results)

    cases = sorted(set(old_values) | set(new_values), key=case_sort_key)
    ratios: dict[CaseKey, float] = {}
    for case in cases:
        name = case_name(case)
        old = old_values.get(case)
        if old is None:
            raise RuntimeError(f"First set of results do not have a value for {name}")
        new = new_values.get(case)
        if new is None:
            raise RuntimeError(f"Second set of results do not have a value for {name}")
        ratios[case] = new / old

    value_widths = {
        name: max(
            len(str(value))
            for case in cases
            for field_name, value in case
            if field_name == name
        )
        for name in {name for case in cases for name, _ in case}
    }
    if include_absolute:
        max_value = max(list(old_values.values()) + list(new_values.values()))
        max_value_len = len(f"{max_value:.2e}")
        value_fmt = f" {{:>{max_value_len}.2e}}".format

    for case in cases:
        name = case_name(case, value_widths)
        ratio = ratios[case]
        if include_absolute:
            old_text = value_fmt(old_values[case])
            new_text = value_fmt(new_values[case])
            print(f"{name}: {old_text} --> {new_text} (x{ratio:>4.2f})")
        else:
            print(f"{name}: x{ratio:>4.2f}")

    print()
    summary_labels = ("Median ratio (lower is better)", "Mean ratio (lower is better)")
    summary_fmt = f"{{:<{max(map(len, summary_labels))}}}: x{{:>4.2f}}".format
    print(summary_fmt(summary_labels[0], statistics.median(ratios.values())))
    print(summary_fmt(summary_labels[1], statistics.mean(ratios.values())))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-a",
        "--include-absolute",
        action="store_true",
        help="include the old and new absolute values (do not use this for commit messages)",
    )
    parser.add_argument("old_file", type=Path, help="file containing old results")
    parser.add_argument("new_file", type=Path, help="file containing new results")
    args = parser.parse_args()

    print(f"Reading data from {args.old_file} and {args.new_file}")
    print_speedups(args.old_file, args.new_file, args.include_absolute)
    return 0


if __name__ == "__main__":
    sys.exit(main())
