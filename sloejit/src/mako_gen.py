#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import argparse
import importlib.util
import os
import sys

from collections.abc import Sequence
from types import ModuleType

from mako.template import Template, exceptions


def relative_import(base_dir: str, path: str) -> ModuleType:
    path = os.path.join(base_dir, path)
    stem = os.path.basename(os.path.splitext(path)[0])
    spec = importlib.util.spec_from_file_location(stem, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"could not import {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("-o", "--output")

    args = parser.parse_args(argv[1:])
    path = os.path.abspath(args.path)
    path_dir = os.path.dirname(path)

    with open(path) as f:
        template = Template(f.read(), strict_undefined=True)

    def load_module(relpath: str) -> ModuleType:
        return relative_import(path_dir, relpath)

    try:
        output = template.render(load_module=load_module)
    except Exception:
        print(exceptions.text_error_template().render())
        return 1

    if args.output:
        with open(args.output, "w") as f:
            f.write(output)
    else:
        print(output)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
