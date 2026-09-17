#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import argparse
import ast
import re
import subprocess
import sys
import tempfile

from dataclasses import dataclass
from pathlib import Path
from typing import cast

CASE_SECTION_PREFIX = ".text.sloejit_case_"
CASE_RE = re.compile(
    r"\bcases\.add_case\(\s*"
    r"(?P<opcode>(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*)\s*,\s*"
    r'(?P<asm>(?:u8|u|U|L)?"(?:\\.|[^"\\])*")\s*,'
)


@dataclass
class TestCase:
    source: Path
    line: int
    expected_opcode: int
    asm: str


def parse_opcode(arg: str) -> int:
    token = arg.strip()
    match = re.fullmatch(r"(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*", token)
    if not match:
        raise ValueError(f"unsupported opcode expression: {arg!r}")
    return int(match.group(1), 0) & 0xFFFFFFFF


def parse_cxx_string(arg: str) -> str:
    token = arg.strip()
    match = re.fullmatch(r'(?:u8|u|U|L)?("(?:\\.|[^"\\])*")', token, re.DOTALL)
    if not match:
        raise ValueError(f"unsupported assembly string expression: {arg!r}")
    return cast(str, ast.literal_eval(match.group(1)))


def scrape_cases(path: Path) -> list[TestCase]:
    cases: list[TestCase] = []

    allow_list = {
        # Q-variant structure loads are in SME2p1, which is not widely
        # supported by compilers. This family have been manually verified
        "ld2q",
        "ld3q",
        "ld4q",
        "st2q",
        "st3q",
        "st4q",
        # Assemblers requires label/relocation syntax for ADRP, not literal
        # immediates as in the adrp.cpp test. ADRP has been manually verified
        "adrp",
    }

    for line_number, line in enumerate(path.read_text().splitlines(), start=1):
        if "cases.add_case" not in line:
            continue

        match = CASE_RE.search(line)
        if not match:
            raise ValueError(
                f"{line_number}: cases.add_case must put the opcode and assembly string on the same line"
            )

        asm_line = parse_cxx_string(match.group("asm"))
        mnemonic = asm_line.split()[0]
        if mnemonic in allow_list:
            continue

        cases.append(
            TestCase(path, line_number, parse_opcode(match.group("opcode")), asm_line)
        )
    return cases


def default_compiler_args(compiler_id: str) -> list[str]:
    if "Clang" in compiler_id:
        return ["--target=aarch64-none-elf"]
    return []


def make_assembly(cases: list[TestCase]) -> str:
    lines = [".arch armv9-a+sve+sme"]
    for idx, case in enumerate(cases):
        source = str(case.source).replace("\\", "\\\\").replace('"', '\\"')
        lines.extend(
            [
                f'.section {CASE_SECTION_PREFIX}{idx},"ax",@progbits',
                ".balign 4",
                f'# {case.line} "{source}"',
                f"\t{case.asm}",
            ]
        )
    lines.append("")
    return "\n".join(lines)


def dump_sections(objdump: str, path: Path) -> dict[str, bytes]:
    command = [objdump, "-s", str(path)]
    result = subprocess.run(command, stdout=subprocess.PIPE, text=True, check=True)

    sections: dict[str, bytearray] = {}
    current_section: str | None = None
    for line in result.stdout.splitlines():
        match = re.fullmatch(r"Contents of section ([^:]+):", line)
        if match:
            section = match.group(1)
            current_section = (
                section if section.startswith(CASE_SECTION_PREFIX) else None
            )
            if current_section is not None:
                sections[current_section] = bytearray()
            continue

        if current_section is None:
            continue
        match = re.match(r"\s*[0-9a-fA-F]+\s+(.*)", line)
        if not match:
            continue
        for word in match.group(1)[:35].split():
            if not re.fullmatch(r"(?:[0-9a-fA-F]{2})+", word):
                break
            sections[current_section].extend(bytes.fromhex(word))

    return {name: bytes(data) for name, data in sections.items()}


def assemble_cases(
    cases: list[TestCase], compiler: str, compiler_args: list[str], objdump: str
) -> dict[str, bytes]:
    with tempfile.TemporaryDirectory(prefix="sloejit-binary-cases-") as tmp:
        tmp_path = Path(tmp)
        asm_path = tmp_path / "cases.s"
        obj_path = tmp_path / "cases.o"
        asm_path.write_text(make_assembly(cases), encoding="utf-8")

        command = [
            compiler,
            *compiler_args,
            "-c",
            "-x",
            "assembler",
            str(asm_path),
            "-o",
            str(obj_path),
        ]
        subprocess.run(command, check=True)
        return dump_sections(objdump, obj_path)


def verify(cases: list[TestCase], sections: dict[str, bytes]) -> int:
    status = 0
    for idx, case in enumerate(cases):
        section_name = f"{CASE_SECTION_PREFIX}{idx}"
        section = sections.get(section_name)
        if section is None:
            print(
                f"{case.source}:{case.line}: missing object section {section_name}",
                file=sys.stderr,
            )
            status = 1
            continue
        if len(section) != 4:
            print(
                f"{case.source}:{case.line}: expected one 4-byte instruction, got {len(section)} bytes for {case.asm!r}",
                file=sys.stderr,
            )
            status = 1
            continue
        actual = int.from_bytes(section, byteorder="little")
        if actual != case.expected_opcode:
            print(
                f"{case.source}:{case.line}: {case.asm!r} assembled to 0x{actual:08x}, expected 0x{case.expected_opcode:08x}",
                file=sys.stderr,
            )
            status = 1
    return status


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify SloeJIT cases.add_case opcode literals against a real AArch64 assembler."
    )
    parser.add_argument(
        "--compiler", required=True, help="compiler used to assemble test cases"
    )
    parser.add_argument(
        "--compiler-id", required=True, help="CMake compiler identifier"
    )
    parser.add_argument(
        "--objdump",
        required=True,
        help="objdump used to read assembled object section contents",
    )
    parser.add_argument("test_files", nargs="+", type=Path)
    args = parser.parse_args()

    cases: list[TestCase] = []
    for path in args.test_files:
        cases.extend(scrape_cases(path))

    if not cases:
        print("No cases.add_case invocations found.", file=sys.stderr)
        return 1

    compiler_args = default_compiler_args(args.compiler_id)
    sections = assemble_cases(cases, args.compiler, compiler_args, args.objdump)

    return verify(cases, sections)


if __name__ == "__main__":
    sys.exit(main())
