#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

"""Run checks on static archives."""

import subprocess

from argparse import ArgumentParser
from collections import Counter
from pathlib import Path


def check_unique_names(archive: Path) -> None:
    result = subprocess.run(
        ["ar", "t", str(archive)],
        check=True,
        text=True,
        capture_output=True,
    )
    members = result.stdout.splitlines()
    counts = Counter(members)
    duplicates = sorted(name for name, count in counts.items() if count > 1)
    assert len(duplicates) == 0, f"duplicate member filenames: {','.join(duplicates)}"


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("archives", nargs="+", type=Path)
    args = parser.parse_args()
    for archive in args.archives:
        check_unique_names(archive)


if __name__ == "__main__":
    main()
