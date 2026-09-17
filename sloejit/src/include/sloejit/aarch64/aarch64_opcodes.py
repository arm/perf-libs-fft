# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

# The set of instructions exposed by aarch64.hpp, a subset of the entire
# AArch64 ISA.

from __future__ import annotations

from collections.abc import Callable
from typing import Any, TypeVar

T = TypeVar("T")


def declarative_registration(
    elems: dict[str, Any], key_attr: str
) -> Callable[[type[T]], type[T]]:
    """
    classes with this annotation are modified such that instances are
    automatically gathered into the specified dict of elems.
    """

    def register(cls: type[T]) -> type[T]:
        # replace the old __init__ method with one that also appends to elems.
        old_init = cls.__init__

        def new_init(self: Any, *args: Any, **kwargs: Any) -> None:
            old_init(self, *args, **kwargs)
            k = getattr(self, key_attr)
            assert k not in elems
            elems[k] = self

        cls.__init__ = new_init  # type: ignore[method-assign]
        return cls

    return register


register_types: dict[str, AArch64RegisterType] = {}
opcodes: dict[str, AArch64InstructionEncoding] = {}


class AArch64EmitAsm:
    fn_name: str
    prefix: str | None

    def __init__(self, fn_name: str, prefix: str | None = None):
        self.fn_name = fn_name
        self.prefix = prefix


@declarative_registration(register_types, "lookup")
class AArch64RegisterType:
    """
    Represents a type of AArch64 register.
    """

    def __init__(
        self,
        lookup: str,
        name: str,
        narrow_name: str | None,
        wide_name: str | None,
        normalize: str,
        *,
        emit_asm: dict[str, AArch64EmitAsm] | None = None,
        is_target: bool = False,
    ):
        assert (narrow_name is None) == (wide_name is None)
        self.lookup = lookup
        self.name = name
        self.narrow_name = narrow_name
        self.wide_name = wide_name
        self.normalize = normalize
        self.emit_asm = emit_asm
        self.is_immediate = narrow_name is None
        self.is_target = is_target
        assert (not self.is_target) or self.is_immediate


class AArch64Operand:

    def __init__(
        self,
        parent: AArch64RegisterType,
        reg_idx: int,
        imm_idx: int,
        *,
        asm_mod: str,
    ):
        self.parent = parent
        self.reg_idx = reg_idx
        self.imm_idx = imm_idx

        # A modifier to index into parent.emit_asm, or empty string for the
        # default.
        self.asm_mod = asm_mod

    def with_asm_mod(self, asm_mod: str) -> AArch64Operand:
        return AArch64Operand(
            self.parent,
            self.reg_idx,
            self.imm_idx,
            asm_mod=asm_mod,
        )

    @property
    def emit_asm(self) -> AArch64EmitAsm | None:
        if self.parent.emit_asm is None:
            return None
        assert (
            self.asm_mod in self.parent.emit_asm
        ), f"No such modifier '{self.asm_mod}' supported: {self.parent.lookup}"
        return self.parent.emit_asm[self.asm_mod]


# fmt: off
# General-purpose regs
AArch64RegisterType('r_12_15', 'r', 'x', 'x', 'normalise_x_12_15', emit_asm={
    'w': AArch64EmitAsm("emit_asm_w"),
})
AArch64RegisterType('r_xzr', 'r', 'x', 'x', 'normalise_x_allow_xzr', emit_asm={
    "": AArch64EmitAsm("emit_asm_x_allow_xzr"),
    ",": AArch64EmitAsm("emit_asm_x_omit_xzr", prefix=", "),
    "r": AArch64EmitAsm("emit_asm_r_allow_rzr_zv"),
    "q": AArch64EmitAsm("emit_asm_r_allow_rzr_qv"),
    "w": AArch64EmitAsm("emit_asm_w_allow_wzr"),
})
AArch64RegisterType('r_sp', 'r', 'x', 'x', 'normalise_x_allow_sp',  emit_asm={
    "": AArch64EmitAsm("emit_asm_x_allow_sp"),
})

# Immediates
AArch64RegisterType('i', 'i', None, None, 'get_literal', emit_asm={
    "": AArch64EmitAsm("emit_asm_literal"),
    ",": AArch64EmitAsm("emit_asm_literal_omit0", prefix=", "),
    "c": AArch64EmitAsm("emit_asm_condition_code"),
    "l": AArch64EmitAsm("emit_asm_integer"),
    "m": AArch64EmitAsm("emit_asm_movx_literal"),
    "t": AArch64EmitAsm("emit_asm_target_label"),
    "z": AArch64EmitAsm("emit_asm_mul_vl"),
    ",z": AArch64EmitAsm("emit_asm_mul_vl_omit0", prefix=", "),
})
AArch64RegisterType('l', 'l', None, None, 'get_literal', emit_asm={
    "": AArch64EmitAsm("emit_asm_integer"),
})
AArch64RegisterType('sh', '', None, None, 'get_literal', emit_asm={
    "": AArch64EmitAsm("emit_asm_lsl"),
    ",": AArch64EmitAsm("emit_asm_lsl_omit_lsl0", prefix=", "),
    "s": AArch64EmitAsm("emit_asm_sxtw_or_lsl_zv"),
})

# Immediates, possibly via relocation targets
AArch64RegisterType('lo12', 'i', None, None, 'get_literal_else_reloc(i, pc, relocs, R_AARCH64_ADD_ABS_LO12_NC)', is_target=True, emit_asm={
    "": AArch64EmitAsm("emit_lo12"),
})
AArch64RegisterType('lo21', 'i', None, None, 'get_literal_else_reloc(i, pc, relocs, R_AARCH64_ADR_PREL_LO21)', is_target=True, emit_asm={
    "": AArch64EmitAsm("emit_asm_target_label"),
})
AArch64RegisterType('hi21', 'i', None, None, 'get_literal_else_reloc(i, pc, relocs, R_AARCH64_ADR_PREL_PG_HI21)', is_target=True, emit_asm={
    "": AArch64EmitAsm("emit_hi21"),
})
AArch64RegisterType('call26', 'i', None, None, 'get_literal_else_reloc(i, pc, relocs, R_AARCH64_CALL26)', is_target=True, emit_asm={
    "": AArch64EmitAsm("emit_asm_target_label"),
})

# Condition code immediate (implicit immediate)
AArch64RegisterType('cond', '', None, None, 'get_literal', emit_asm={
    "c": AArch64EmitAsm("emit_asm_condition_code"),
})

# Special immediate for smstart/smstop options
AArch64RegisterType('smopt', 'i', None, None, 'get_literal', emit_asm={
    "": AArch64EmitAsm("emit_asm_smopt"),
})

AArch64RegisterType('hv', 'o', None, None, 'get_literal', emit_asm={
    "": AArch64EmitAsm("emit_asm_hv"),
})

# Neon/SVE size suffixes (implicit immediates)
AArch64RegisterType('qv', '', None, None, '(aarch64::q_type_variant) get_literal', emit_asm={
    "": AArch64EmitAsm("get_qv_long_str"),
    "l": AArch64EmitAsm("get_qv_short_str"),
})
AArch64RegisterType('zv', '', None, None, '(aarch64::z_type_variant) get_literal', emit_asm={
    "": AArch64EmitAsm("get_zv_str"),
})
AArch64RegisterType('ptrue_pat', '', None, None, '(aarch64::ptrue_pat) get_literal', emit_asm={
    "": AArch64EmitAsm("get_ptrue_pat_str"),
})

# Predicate regs
AArch64RegisterType('p_lo', 'p', 'p', 'p', 'normalise_p_low8', emit_asm={
    "": AArch64EmitAsm("emit_asm_p"),
})
AArch64RegisterType('p_all', 'p', 'p', 'p', 'normalise_p_all16', emit_asm={
    "": AArch64EmitAsm("emit_asm_p"),
})

# SIMD regs
AArch64RegisterType('b', 'b', 'b', 'z', 'normalise_b', emit_asm={
    "": AArch64EmitAsm("emit_asm_b"),
})
AArch64RegisterType('h', 'h', 'h', 'z', 'normalise_h', emit_asm={
    "": AArch64EmitAsm("emit_asm_h"),
})
AArch64RegisterType('s', 's', 's', 'z', 'normalise_s', emit_asm={
    "": AArch64EmitAsm("emit_asm_s"),
})
AArch64RegisterType('d', 'd', 'd', 'z', 'normalise_d', emit_asm={
    "": AArch64EmitAsm("emit_asm_d"),
})
AArch64RegisterType('q', 'q', 'q', 'z', 'normalise_q', emit_asm={
    "": AArch64EmitAsm("emit_asm_q"),
    "v": AArch64EmitAsm("emit_asm_v"),
})
AArch64RegisterType('z', 'z', 'z', 'z', 'normalise_z', emit_asm={
    "": AArch64EmitAsm("emit_asm_z"),
})

# za
AArch64RegisterType('za', 'a', 'za', 'za', 'normalise_za', emit_asm={
    "": AArch64EmitAsm("emit_asm_za"),
})

# fmt: on


AsmPart = str | AArch64Operand


def append_asm_str(xs: list[AsmPart], s: str) -> list[AsmPart]:
    if len(xs) >= 1 and type(xs[-1]) is str:
        xs[-1] += s
    else:
        xs.append(s)
    return xs


def parse_asm_pattern(s: str, operands: list[AArch64Operand]) -> list[AsmPart]:
    # emit_asm formatters are represented in a string format, so before they
    # can be applied in aarch64.cpp.mako they must first be parsed into
    # individual parts.
    # This function returns a list of parts, where each part is either an
    # AArch64Operand (possibly with a modifier applied), or a str.
    # For example, consider the following emit_asm input string:
    # AArch64InstructionEncoding("ld1w", ["=z", "p_lo", "r_sp", "i", "zv"],
    #                            emit_asm="ld1w\t{%0.%4}, %1/z, [%2%,z3]")
    # This is split into the following parts:
    # ["ld1w\t{", part0, ".", part1, "}, ", part2, "/z, [", part3, part4, "]"]
    # Where part0, part1, part2, and part3 are all printed with the default
    # formatters for their respective operands.
    # The part4 operand string contains modifier characters and is interpreted
    # as follows:
    #    %,z3
    #       ^refers to operand 3.
    #     ^^uses the ",z" emit_asm modifier for that operand (emit_asm_mul_vl).
    #       the convention is that operand modifiers with commas include ", "
    #       as a prefix passed to the emit C++ function such that they are
    #       optional when printing.
    ret: list[AsmPart] = [""]
    while s != "":
        # Handle escaped % literals.
        if s[:2] == "%%":
            ret = append_asm_str(ret, "%")
            s = s[2:]
            continue

        # Handle regimm %N patterns, possibly with modifiers.
        if s[0] == "%":
            mod = ""
            # Handle regimm modifier characters before the operand number.
            while not s[1].isdigit():
                mod += s[1]
                s = s[1:]
            ret.append(operands[int(s[1])].with_asm_mod(mod))
            s = s[2:]
            continue

        # No pattern, just take character as-is.
        ret = append_asm_str(ret, s[0])
        s = s[1:]

    return ret


def parse_asm_patterns(
    mnemonic: str,
    emit_asms: str | dict[str, str] | None,
    operands: list[AArch64Operand],
) -> dict[str, list[AsmPart]]:
    if type(emit_asms) is dict:
        return {
            name: parse_asm_pattern(emit_asm, operands)
            for name, emit_asm in emit_asms.items()
        }
    elif type(emit_asms) is str:
        return {"": parse_asm_pattern(emit_asms, operands)}

    assert emit_asms is None
    # Synthesize a default emit_asm.
    emit_asm: list[AsmPart] = [mnemonic]
    for i, operand in enumerate(operands):
        emit_asm.append(", " if i > 0 else "\t")
        emit_asm.append(operand)
    return {"": emit_asm}


def split_reg_modifiers(
    xs: list[str],
) -> tuple[list[AArch64Operand], list[AArch64RegisterType], list[str]]:
    inout = []
    operands = []
    regs = []
    reg_idx = 0
    imm_idx = 0
    for x in xs:
        io = ""
        if x[0] in ["+", "="]:
            io = x[0]
            x = x[1:]
        reg = register_types[x]
        # Use canonical printing by default, possibly overloaded in parsed
        # emit_asm str.
        operands.append(AArch64Operand(reg, reg_idx, imm_idx, asm_mod=""))
        if reg.is_immediate:
            assert io == ""
            imm_idx += 1
            continue
        reg_idx += 1
        regs.append(reg)
        inout.append(io)
    return (operands, regs, inout)


@declarative_registration(opcodes, "name")
class AArch64InstructionEncoding:
    """
    Represents a particular set AArch64 instruction encoding for a given
    mnemonic and set of register parameters.
    """

    def __init__(
        self,
        mnemonic: str,
        operand_strs: list[str],
        *,
        kind: str = "IK_NORMAL",
        clobbers: str | None = None,
        emit_asm: str | dict[str, str] | None = None,
    ):
        operands, regs, inout = split_reg_modifiers(operand_strs)
        self.name = "{}_{}".format(
            mnemonic, "".join(reg.parent.name for reg in operands)
        )
        self.mnemonic = mnemonic
        self.regs = regs
        self.operands = operands
        self.inout = inout
        self.kind = kind
        self.clobbers = clobbers
        self.emit_asms = parse_asm_patterns(mnemonic, emit_asm, self.operands)


# fmt: off
# Branch-with-link calls are modeled as normal instructions that just happen
# to clobber a lot of registers, since they don't actually change control-flow
# from executing the next instruction.
call_clobber = "get_pcs_clobbered()"
AArch64InstructionEncoding("blr", ["r_xzr"], clobbers=call_clobber)
AArch64InstructionEncoding("bl", ["call26"], clobbers=call_clobber)

# Other branch instructions, including ret!
AArch64InstructionEncoding("b_cond", ["cond", "i"], kind="IK_COND_BRANCH_PCREL", emit_asm="b.%c0\t%t1")
AArch64InstructionEncoding("b", ["i"], kind="IK_BRANCH_PCREL")
AArch64InstructionEncoding("cbnz", ["r_xzr", "i"], kind="IK_COND_BRANCH_PCREL")
AArch64InstructionEncoding("cbz", ["r_xzr", "i"], kind="IK_COND_BRANCH_PCREL")
AArch64InstructionEncoding("ret", [], kind="IK_RETURN")

# Everything else is just a normal data-processing instruction.
AArch64InstructionEncoding("add", ["=r_sp", "r_sp", "lo12"])
AArch64InstructionEncoding("add", ["=r_xzr", "r_xzr", "r_xzr", "sh"], emit_asm="add\t%0, %1, %2%,3")
AArch64InstructionEncoding("add", ["=q", "q", "q", "qv"], emit_asm="add\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("add", ["+z", "i", "zv"], emit_asm="add\t%0.%2, %0.%2, %1")
AArch64InstructionEncoding("add", ["+z", "p_lo", "z", "zv"], emit_asm="add\t%0.%3, %1/m, %0.%3, %2.%3")
AArch64InstructionEncoding("add", ["=z", "z", "z", "zv"], emit_asm="add\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("adds", ["=r_xzr", "r_sp", "i"])
AArch64InstructionEncoding("adds", ["=r_xzr", "r_xzr", "r_xzr"])
AArch64InstructionEncoding("addsvl", ["=r_sp", "r_sp", "i"])
AArch64InstructionEncoding("addvl", ["=r_sp", "r_sp", "i"])
AArch64InstructionEncoding("adr", ["=r_xzr", "lo21"])
AArch64InstructionEncoding("adrp", ["=r_xzr", "hi21"])
AArch64InstructionEncoding("and", ["=r_xzr", "r_xzr", "r_xzr"])
AArch64InstructionEncoding("and", ["=q", "q", "q", "qv"], emit_asm="and\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("and", ["=z", "z", "z"], emit_asm="and\t%0.d, %1.d, %2.d")
AArch64InstructionEncoding("asr", ["=r_xzr", "r_xzr", "i"])
AArch64InstructionEncoding("cntb", ["=r_xzr"])
AArch64InstructionEncoding("cntd", ["=r_xzr"])
AArch64InstructionEncoding("cnth", ["=r_xzr"])
AArch64InstructionEncoding("cntp", ["=r_xzr", "p_all", "p_all", "zv"], emit_asm="cntp\t%0, %1, %2.%3")
AArch64InstructionEncoding("cntw", ["=r_xzr"])
AArch64InstructionEncoding("csel", ["=r_xzr", "r_xzr", "r_xzr", "i"], emit_asm="csel\t%0, %1, %2, %c3")
AArch64InstructionEncoding("dup", ["=z", "i", "zv"], emit_asm="dup\t%0.%2, %1")
AArch64InstructionEncoding("dup", ["=z", "z", "l", "zv"], emit_asm="dup\t%0.%3, %1.%3[%2]")
AArch64InstructionEncoding("dup", ["=q", "r_xzr", "qv"], emit_asm="dup\t%v0.%2, %q1")
AArch64InstructionEncoding("dup", ["=q", "q", "l", "qv"], emit_asm="dup\t%v0.%3, %v1.%l3[%2]")
AArch64InstructionEncoding("ext", ["=q", "q", "q", "i", "qv"], emit_asm="ext\t%v0.%4, %v1.%4, %v2.%4, %3")
AArch64InstructionEncoding("ext", ["+z", "z", "i"], emit_asm="ext\t%0.b, %0.b, %1.b, %2")
AArch64InstructionEncoding("fadd", ["=d", "d", "d"])
AArch64InstructionEncoding("fadd", ["=h", "h", "h"])
AArch64InstructionEncoding("fadd", ["=q", "q", "q", "qv"], emit_asm="fadd\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("fadd", ["=s", "s", "s"])
AArch64InstructionEncoding("fadd", ["+z", "p_lo", "z", "zv"], emit_asm="fadd\t%0.%3, %1/m, %0.%3, %2.%3")
AArch64InstructionEncoding("fadd", ["=z", "z", "z", "zv"], emit_asm="fadd\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("fcmla", ["+q", "q", "q", "i", "qv"], emit_asm="fcmla\t%v0.%4, %v1.%4, %v2.%4, %3")
AArch64InstructionEncoding("fcmla", ["+z", "p_lo", "z", "z", "i", "zv"], emit_asm="fcmla\t%0.%5, %1/m, %2.%5, %3.%5, %4")
AArch64InstructionEncoding("fcvt", ["=d", "h"])
AArch64InstructionEncoding("fcvt", ["=d", "s"])
AArch64InstructionEncoding("fcvt", ["=h", "d"])
AArch64InstructionEncoding("fcvt", ["=h", "s"])
AArch64InstructionEncoding("fcvt", ["=s", "d"])
AArch64InstructionEncoding("fcvt", ["=s", "h"])
AArch64InstructionEncoding("fcvtl", ["=q", "q", "qv", "qv"], emit_asm="fcvtl\t%v0.%2, %v1.%3")
AArch64InstructionEncoding("fcvtn", ["=q", "q", "qv", "qv"], emit_asm="fcvtn\t%v0.%2, %v1.%3")
AArch64InstructionEncoding("fmla", ["+q", "q", "q", "l", "qv"], emit_asm="fmla\t%v0.%4, %v1.%4, %v2.%l4[%3]")
AArch64InstructionEncoding("fmla", ["+q", "q", "q", "qv"], emit_asm="fmla\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("fmla", ["+z", "p_lo", "z", "z", "zv"], emit_asm="fmla\t%0.%4, %1/m, %2.%4, %3.%4")
AArch64InstructionEncoding("fmla", ["+z", "z", "z", "l", "zv"], emit_asm="fmla\t%0.%4, %1.%4, %2.%4[%3]")
AArch64InstructionEncoding("fmls", ["+q", "q", "q", "l", "qv"], emit_asm="fmls\t%v0.%4, %v1.%4, %v2.%l4[%3]")
AArch64InstructionEncoding("fmls", ["+q", "q", "q", "qv"], emit_asm="fmls\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("fmls", ["+z", "p_lo", "z", "z", "zv"], emit_asm="fmls\t%0.%4, %1/m, %2.%4, %3.%4")
AArch64InstructionEncoding("fmls", ["+z", "z", "z", "l", "zv"], emit_asm="fmls\t%0.%4, %1.%4, %2.%4[%3]")
AArch64InstructionEncoding("fmov", ["=d", "r_xzr"])
AArch64InstructionEncoding("fmov", ["=h", "r_xzr"], emit_asm="fmov\t%0, %w1")
AArch64InstructionEncoding("fmov", ["=r_xzr", "d"])
AArch64InstructionEncoding("fmov", ["=r_xzr", "h"], emit_asm="fmov\t%w0, %1")
AArch64InstructionEncoding("fmov", ["=r_xzr", "s"], emit_asm="fmov\t%w0, %1")
AArch64InstructionEncoding("fmov", ["=s", "r_xzr"], emit_asm="fmov\t%0, %w1")
AArch64InstructionEncoding("smov", ["=r_xzr", "q", "l", "qv"], emit_asm="smov\t%0, %v1.%l3[%2]")
AArch64InstructionEncoding("fmul", ["=d", "d", "d"])
AArch64InstructionEncoding("fmul", ["=h", "h", "h"])
AArch64InstructionEncoding("fmul", ["=q", "q", "q", "l", "qv"], emit_asm="fmul\t%v0.%4, %v1.%4, %v2.%l4[%3]")
AArch64InstructionEncoding("fmul", ["=q", "q", "q", "qv"], emit_asm="fmul\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("fmul", ["=s", "s", "s"])
AArch64InstructionEncoding("fmul", ["+z", "p_lo", "z", "zv"], emit_asm="fmul\t%0.%3, %1/m, %0.%3, %2.%3")
AArch64InstructionEncoding("fmul", ["=z", "z", "z", "zv"], emit_asm="fmul\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("fneg", ["=d", "d"])
AArch64InstructionEncoding("fneg", ["=h", "h"])
AArch64InstructionEncoding("fneg", ["=q", "q", "qv"], emit_asm="fneg\t%v0.%2, %v1.%2")
AArch64InstructionEncoding("fneg", ["=s", "s"])
AArch64InstructionEncoding("fneg", ["+z", "p_lo", "z", "zv"], emit_asm="fneg\t%0.%3, %1/m, %2.%3")
AArch64InstructionEncoding("fsub", ["=d", "d", "d"])
AArch64InstructionEncoding("fsub", ["=h", "h", "h"])
AArch64InstructionEncoding("fsub", ["=q", "q", "q", "qv"], emit_asm="fsub\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("fsub", ["=s", "s", "s"])
AArch64InstructionEncoding("fsub", ["+z", "p_lo", "z", "zv"], emit_asm="fsub\t%0.%3, %1/m, %0.%3, %2.%3")
AArch64InstructionEncoding("fsub", ["=z", "z", "z", "zv"], emit_asm="fsub\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("incb", ["+r_xzr"])
AArch64InstructionEncoding("incd", ["+r_xzr"])
AArch64InstructionEncoding("inch", ["+r_xzr"])
AArch64InstructionEncoding("incw", ["+r_xzr"])
AArch64InstructionEncoding("index", ["=z", "i", "i", "zv"], emit_asm="index\t%0.%3, %1, %2")
AArch64InstructionEncoding("index", ["=z", "i", "r_xzr", "zv"], emit_asm="index\t%0.%3, %1, %r2")
AArch64InstructionEncoding("index", ["=z", "r_xzr", "i", "zv"], emit_asm="index\t%0.%3, %r1, %2")
AArch64InstructionEncoding("index", ["=z", "r_xzr", "r_xzr", "zv"], emit_asm="index\t%0.%3, %r1, %r2")
AArch64InstructionEncoding("ld1b", ["=z", "p_lo", "r_sp", "i", "zv"], emit_asm="ld1b\t{%0.%4}, %1/z, [%2%,z3]")
AArch64InstructionEncoding("ld1b", ["=z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="ld1b\t{%0.%4}, %1/z, [%2, %3]")
AArch64InstructionEncoding("ld1b", ["=z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="ld1b\t{%0.%5}, %1/z, [%2, %3.%5, %4]")
AArch64InstructionEncoding("ld1b", ["=za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "ld1b\t{%0%1%2.b[%w3, %4]}, %5/z, [%6, %7]",
        "no_offset": "ld1b\t{%0%1%2.b[%w3, %4]}, %5/z, [%6]"})
AArch64InstructionEncoding("ld1d", ["=z", "p_lo", "r_sp", "i", "zv"], emit_asm="ld1d\t{%0.%4}, %1/z, [%2%,z3]")
AArch64InstructionEncoding("ld1d", ["=z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="ld1d\t{%0.%4}, %1/z, [%2, %3, lsl #3]")
AArch64InstructionEncoding("ld1d", ["=z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="ld1d\t{%0.%5}, %1/z, [%2, %3.%5, %4]")
AArch64InstructionEncoding("ld1d", ["=za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "ld1d\t{%0%1%2.d[%w3, %4]}, %5/z, [%6, %7, lsl #3]",
        "no_offset": "ld1d\t{%0%1%2.d[%w3, %4]}, %5/z, [%6]"})
AArch64InstructionEncoding("ld1h", ["=z", "p_lo", "r_sp", "i", "zv"], emit_asm="ld1h\t{%0.%4}, %1/z, [%2%,z3]")
AArch64InstructionEncoding("ld1h", ["=z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="ld1h\t{%0.%4}, %1/z, [%2, %3, lsl #1]")
AArch64InstructionEncoding("ld1h", ["=z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="ld1h\t{%0.%5}, %1/z, [%2, %3.%5, %s4]")
AArch64InstructionEncoding("ld1h", ["=za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "ld1h\t{%0%1%2.h[%w3, %4]}, %5/z, [%6, %7, lsl #1]",
        "no_offset": "ld1h\t{%0%1%2.h[%w3, %4]}, %5/z, [%6]"})
AArch64InstructionEncoding("ld1q", ["=za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "ld1q\t{%0%1%2.q[%w3, %4]}, %5/z, [%6, %7, lsl #4]",
        "no_offset": "ld1q\t{%0%1%2.q[%w3, %4]}, %5/z, [%6]"})
AArch64InstructionEncoding("ld1r", ["=q", "r_sp", "qv"], emit_asm="ld1r\t{%v0.%2}, [%1]")
AArch64InstructionEncoding("ld2", ["=q", "=q", "r_sp", "qv"], emit_asm="ld2\t{%v0.%3, %v1.%3}, [%2]")
AArch64InstructionEncoding("ld3", ["=q", "=q", "=q", "r_sp", "qv"], emit_asm="ld3\t{%v0.%4, %v1.%4, %v2.%4}, [%3]")
AArch64InstructionEncoding("ld4", ["=q", "=q", "=q", "=q", "r_sp", "qv"], emit_asm="ld4\t{%v0.%5, %v1.%5, %v2.%5, %v3.%5}, [%4]")
AArch64InstructionEncoding("st2", ["q", "q", "r_sp", "qv"], emit_asm="st2\t{%v0.%3, %v1.%3}, [%2]")
AArch64InstructionEncoding("st3", ["q", "q", "q", "r_sp", "qv"], emit_asm="st3\t{%v0.%4, %v1.%4, %v2.%4}, [%3]")
AArch64InstructionEncoding("st4", ["q", "q", "q", "q", "r_sp", "qv"], emit_asm="st4\t{%v0.%5, %v1.%5, %v2.%5, %v3.%5}, [%4]")
AArch64InstructionEncoding("ld1rb", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rb\t{%0.b}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rd", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rd\t{%0.d}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rh", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rh\t{%0.h}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rqb", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rqb\t{%0.b}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rqd", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rqd\t{%0.d}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rqh", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rqh\t{%0.h}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rqw", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rqw\t{%0.s}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1rw", ["=z", "p_lo", "r_sp", "i"], emit_asm="ld1rw\t{%0.s}, %1/z, [%2%,3]")
AArch64InstructionEncoding("ld1w", ["=z", "p_lo", "r_sp", "i", "zv"], emit_asm="ld1w\t{%0.%4}, %1/z, [%2%,z3]")
AArch64InstructionEncoding("ld1w", ["=z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="ld1w\t{%0.%4}, %1/z, [%2, %3, lsl #2]")
AArch64InstructionEncoding("ld1w", ["=z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="ld1w\t{%0.%5}, %1/z, [%2, %3.%5, %s4]")
AArch64InstructionEncoding("ld1w", ["=za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "ld1w\t{%0%1%2.s[%w3, %4]}, %5/z, [%6, %7, lsl #2]",
        "no_offset": "ld1w\t{%0%1%2.s[%w3, %4]}, %5/z, [%6]"})
AArch64InstructionEncoding("ld2b", ["=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld2b\t{%0.b, %1.b}, %2/z, [%3%,z4]")
AArch64InstructionEncoding("ld2b", ["=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld2b\t{%0.b, %1.b}, %2/z, [%3, %4]")
AArch64InstructionEncoding("ld2h", ["=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld2h\t{%0.h, %1.h}, %2/z, [%3%,z4]")
AArch64InstructionEncoding("ld2h", ["=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld2h\t{%0.h, %1.h}, %2/z, [%3, %4, lsl #1]")
AArch64InstructionEncoding("ld2w", ["=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld2w\t{%0.s, %1.s}, %2/z, [%3%,z4]")
AArch64InstructionEncoding("ld2w", ["=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld2w\t{%0.s, %1.s}, %2/z, [%3, %4, lsl #2]")
AArch64InstructionEncoding("ld2d", ["=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld2d\t{%0.d, %1.d}, %2/z, [%3%,z4]")
AArch64InstructionEncoding("ld2d", ["=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld2d\t{%0.d, %1.d}, %2/z, [%3, %4, lsl #3]")
AArch64InstructionEncoding("ld2q", ["=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld2q\t{%0.q, %1.q}, %2/z, [%3%,z4]")
AArch64InstructionEncoding("ld2q", ["=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld2q\t{%0.q, %1.q}, %2/z, [%3, %4, lsl #4]")
AArch64InstructionEncoding("ld3b", ["=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld3b\t{%0.b, %1.b, %2.b}, %3/z, [%4%,z5]")
AArch64InstructionEncoding("ld3b", ["=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld3b\t{%0.b, %1.b, %2.b}, %3/z, [%4, %5]")
AArch64InstructionEncoding("ld3h", ["=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld3h\t{%0.h, %1.h, %2.h}, %3/z, [%4%,z5]")
AArch64InstructionEncoding("ld3h", ["=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld3h\t{%0.h, %1.h, %2.h}, %3/z, [%4, %5, lsl #1]")
AArch64InstructionEncoding("ld3w", ["=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld3w\t{%0.s, %1.s, %2.s}, %3/z, [%4%,z5]")
AArch64InstructionEncoding("ld3w", ["=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld3w\t{%0.s, %1.s, %2.s}, %3/z, [%4, %5, lsl #2]")
AArch64InstructionEncoding("ld3d", ["=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld3d\t{%0.d, %1.d, %2.d}, %3/z, [%4%,z5]")
AArch64InstructionEncoding("ld3d", ["=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld3d\t{%0.d, %1.d, %2.d}, %3/z, [%4, %5, lsl #3]")
AArch64InstructionEncoding("ld3q", ["=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld3q\t{%0.q, %1.q, %2.q}, %3/z, [%4%,z5]")
AArch64InstructionEncoding("ld3q", ["=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld3q\t{%0.q, %1.q, %2.q}, %3/z, [%4, %5, lsl #4]")
AArch64InstructionEncoding("ld4b", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld4b\t{%0.b, %1.b, %2.b, %3.b}, %4/z, [%5%,z6]")
AArch64InstructionEncoding("ld4b", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld4b\t{%0.b, %1.b, %2.b, %3.b}, %4/z, [%5, %6]")
AArch64InstructionEncoding("ld4h", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld4h\t{%0.h, %1.h, %2.h, %3.h}, %4/z, [%5%,z6]")
AArch64InstructionEncoding("ld4h", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld4h\t{%0.h, %1.h, %2.h, %3.h}, %4/z, [%5, %6, lsl #1]")
AArch64InstructionEncoding("ld4w", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld4w\t{%0.s, %1.s, %2.s, %3.s}, %4/z, [%5%,z6]")
AArch64InstructionEncoding("ld4w", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld4w\t{%0.s, %1.s, %2.s, %3.s}, %4/z, [%5, %6, lsl #2]")
AArch64InstructionEncoding("ld4d", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld4d\t{%0.d, %1.d, %2.d, %3.d}, %4/z, [%5%,z6]")
AArch64InstructionEncoding("ld4d", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld4d\t{%0.d, %1.d, %2.d, %3.d}, %4/z, [%5, %6, lsl #3]")
AArch64InstructionEncoding("ld4q", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "i"], emit_asm="ld4q\t{%0.q, %1.q, %2.q, %3.q}, %4/z, [%5%,z6]")
AArch64InstructionEncoding("ld4q", ["=z", "=z", "=z", "=z", "p_lo", "r_sp", "r_xzr"], emit_asm="ld4q\t{%0.q, %1.q, %2.q, %3.q}, %4/z, [%5, %6, lsl #4]")
AArch64InstructionEncoding("ldp_d", ["=d", "=d", "r_sp", "i"], emit_asm="ldp\t%0, %1, [%2%,3]")
AArch64InstructionEncoding("ldp_q", ["=q", "=q", "r_sp", "i"], emit_asm="ldp\t%0, %1, [%2%,3]")
AArch64InstructionEncoding("ldp_s", ["=s", "=s", "r_sp", "i"], emit_asm="ldp\t%0, %1, [%2%,3]")
AArch64InstructionEncoding("ldp_x", ["=r_xzr", "=r_xzr", "r_sp", "i"], emit_asm="ldp\t%0, %1, [%2%,3]")
AArch64InstructionEncoding("ldp_x_postindex", ["=r_xzr", "=r_xzr", "+r_sp", "i"], emit_asm="ldp\t%0, %1, [%2], %3")
AArch64InstructionEncoding("ldp_x_preindex", ["=r_xzr", "=r_xzr", "+r_sp", "i"], emit_asm="ldp\t%0, %1, [%2, %3]!")
AArch64InstructionEncoding("ldr", ["=z", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,z2]")
AArch64InstructionEncoding("ldr_b", ["=b", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldr_b", ["=b", "r_sp", "r_xzr"], emit_asm="ldr\t%0, [%1, %2]")
AArch64InstructionEncoding("ldr_d", ["=d", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldr_d", ["=d", "r_sp", "r_xzr"], emit_asm="ldr\t%0, [%1, %2, lsl #3]")
AArch64InstructionEncoding("ldr_h", ["=h", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldr_h", ["=h", "r_sp", "r_xzr"], emit_asm="ldr\t%0, [%1, %2, lsl #1]")
AArch64InstructionEncoding("ldr_q", ["=q", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldr_q", ["=q", "r_sp", "r_xzr"], emit_asm="ldr\t%0, [%1, %2, lsl #4]")
AArch64InstructionEncoding("ldr_s", ["=s", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldr_s", ["=s", "r_sp", "r_xzr"], emit_asm="ldr\t%0, [%1, %2, lsl #2]")
AArch64InstructionEncoding("ldr_x", ["=r_xzr", "r_sp", "i"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldr_x", ["=r_xzr", "r_sp", "r_xzr"], emit_asm="ldr\t%0, [%1%,2]")
AArch64InstructionEncoding("ldrsb_x", ["=r_xzr", "r_sp", "i"], emit_asm="ldrsb\t%0, [%1%,2]")
AArch64InstructionEncoding("ldrsb_x", ["=r_xzr", "r_sp", "r_xzr"], emit_asm="ldrsb\t%0, [%1, %2]")
AArch64InstructionEncoding("lsl", ["=r_xzr", "r_xzr", "i"])
AArch64InstructionEncoding("lsr", ["=r_xzr", "r_xzr", "i"])
AArch64InstructionEncoding("madd", ["=r_xzr", "r_xzr", "r_xzr", "r_xzr"], emit_asm={
        "as_madd": "madd\t%0, %1, %2, %3",
        "as_mul": "mul\t%0, %1, %2"})
AArch64InstructionEncoding("mova", ["=z", "p_lo", "za", "l", "hv", "r_12_15", "l", "zv"], emit_asm="mova\t%0.%7, %1/m, %2%3%4.%7[%w5, %6]")
AArch64InstructionEncoding("mova", ["=za", "l", "hv", "r_12_15", "l", "p_lo", "z", "zv"], emit_asm="mova\t%0%1%2.%7[%w3, %4], %5/m, %6.%7")
AArch64InstructionEncoding("movk", ["+r_xzr", "i"], emit_asm="movk\t%0, %m1")
AArch64InstructionEncoding("movn", ["=r_xzr", "i"], emit_asm="movn\t%0, %m1")
AArch64InstructionEncoding("movz", ["=r_xzr", "i"], emit_asm="movz\t%0, %m1")
AArch64InstructionEncoding("mul", ["+z", "p_lo", "z", "zv"], emit_asm="mul\t%0.%3, %1/m, %0.%3, %2.%3")
AArch64InstructionEncoding("orr", ["=r_xzr", "r_xzr", "r_xzr"], emit_asm={
        "as_orr": "orr\t%0, %1, %2",
        "as_mov": "mov\t%0, %2",  # if %1 == xzr
    })
AArch64InstructionEncoding("orr", ["=q", "q", "q", "qv"], emit_asm={
        "as_orr": "orr\t%v0.%3, %v1.%3, %v2.%3",
        "as_mov": "mov\t%v0.%3, %v1.%3",  # if %1 == %2
    })
AArch64InstructionEncoding("orr", ["=z", "z", "z"], emit_asm={
        "as_orr": "orr\t%0.d, %1.d, %2.d",
        "as_mov": "mov\t%0.d, %1.d",  # if %1 == %2
    })
AArch64InstructionEncoding("pfalse", ["=p_all"], emit_asm="pfalse\t%0.b")
AArch64InstructionEncoding("ptrue", ["=p_all", "ptrue_pat", "zv"], emit_asm={
        "with_pattern": "ptrue\t%0.%2, %1",
        "no_pattern": "ptrue\t%0.%2",  # if %1 == all
    })
AArch64InstructionEncoding("rev16", ["=q", "q", "qv"], emit_asm="rev16\t%v0.%2, %v1.%2")
AArch64InstructionEncoding("rev32", ["=q", "q", "qv"], emit_asm="rev32\t%v0.%2, %v1.%2")
AArch64InstructionEncoding("rev64", ["=q", "q", "qv"], emit_asm="rev64\t%v0.%2, %v1.%2")
AArch64InstructionEncoding("revb", ["+z", "p_lo", "z", "zv"], emit_asm="revb\t%0.%3, %1/m, %2.%3")
AArch64InstructionEncoding("revh", ["+z", "p_lo", "z", "zv"], emit_asm="revh\t%0.%3, %1/m, %2.%3")
AArch64InstructionEncoding("revw", ["+z", "p_lo", "z", "zv"], emit_asm="revw\t%0.%3, %1/m, %2.%3")
AArch64InstructionEncoding("rev", ["=z", "z", "zv"], emit_asm="rev\t%0.%2, %1.%2")
AArch64InstructionEncoding("rev", ["=p_all", "p_all", "zv"], emit_asm="rev\t%0.%2, %1.%2")
AArch64InstructionEncoding("sdiv", ["=r_xzr", "r_xzr", "r_xzr"])
AArch64InstructionEncoding("smlal", ["+q", "q", "q", "qv", "qv"], emit_asm="smlal\t%v0.%3, %v1.%4, %v2.%4")
AArch64InstructionEncoding("smlal", ["+q", "q", "q", "l", "qv", "qv"], emit_asm="smlal\t%v0.%4, %v1.%5, %v2.%l5[%3]")
AArch64InstructionEncoding("smlal2", ["+q", "q", "q", "qv", "qv"], emit_asm="smlal2\t%v0.%3, %v1.%4, %v2.%4")
AArch64InstructionEncoding("smlal2", ["+q", "q", "q", "l", "qv", "qv"], emit_asm="smlal2\t%v0.%4, %v1.%5, %v2.%l5[%3]")
AArch64InstructionEncoding("smlalb", ["=z", "z", "z", "zv", "zv"], emit_asm="smlalb\t%0.%3, %1.%4, %2.%4")
AArch64InstructionEncoding("smlalb", ["=z", "z", "z", "l", "zv", "zv"], emit_asm="smlalb\t%0.%4, %1.%5, %2.%5[%3]")
AArch64InstructionEncoding("smlalt", ["=z", "z", "z", "zv", "zv"], emit_asm="smlalt\t%0.%3, %1.%4, %2.%4")
AArch64InstructionEncoding("smlalt", ["=z", "z", "z", "l", "zv", "zv"], emit_asm="smlalt\t%0.%4, %1.%5, %2.%5[%3]")
AArch64InstructionEncoding("smlsl", ["+q", "q", "q", "qv", "qv"], emit_asm="smlsl\t%v0.%3, %v1.%4, %v2.%4")
AArch64InstructionEncoding("smlsl", ["+q", "q", "q", "l", "qv", "qv"], emit_asm="smlsl\t%v0.%4, %v1.%5, %v2.%l5[%3]")
AArch64InstructionEncoding("smlsl2", ["+q", "q", "q", "qv", "qv"], emit_asm="smlsl2\t%v0.%3, %v1.%4, %v2.%4")
AArch64InstructionEncoding("smlsl2", ["+q", "q", "q", "l", "qv", "qv"], emit_asm="smlsl2\t%v0.%4, %v1.%5, %v2.%l5[%3]")
AArch64InstructionEncoding("smlslb", ["=z", "z", "z", "zv", "zv"], emit_asm="smlslb\t%0.%3, %1.%4, %2.%4")
AArch64InstructionEncoding("smlslb", ["=z", "z", "z", "l", "zv", "zv"], emit_asm="smlslb\t%0.%4, %1.%5, %2.%5[%3]")
AArch64InstructionEncoding("smlslt", ["=z", "z", "z", "zv", "zv"], emit_asm="smlslt\t%0.%3, %1.%4, %2.%4")
AArch64InstructionEncoding("smlslt", ["=z", "z", "z", "l", "zv", "zv"], emit_asm="smlslt\t%0.%4, %1.%5, %2.%5[%3]")
AArch64InstructionEncoding("smstart", ["smopt"], clobbers="get_sm_clobbered()", emit_asm={
        "with_suffix": "smstart\t%0",
        "no_suffix": "smstart",  # if smopt == both
    })
AArch64InstructionEncoding("smstop", ["smopt"], clobbers="get_sm_clobbered()", emit_asm={
        "with_suffix": "smstop\t%0",
        "no_suffix": "smstop",  # if smopt == both
    })
AArch64InstructionEncoding("smull", ["=q", "q", "q", "qv", "qv"], emit_asm="smull\t%v0.%3, %v1.%4, %v2.%4")
AArch64InstructionEncoding("smull", ["=q", "q", "q", "l", "qv", "qv"], emit_asm="smull\t%v0.%4, %v1.%5, %v2.%l5[%3]")
AArch64InstructionEncoding("smull2", ["=q", "q", "q", "qv", "qv"], emit_asm="smull2\t%v0.%3, %v1.%4, %v2.%4")
AArch64InstructionEncoding("smull2", ["=q", "q", "q", "l", "qv", "qv"], emit_asm="smull2\t%v0.%4, %v1.%5, %v2.%l5[%3]")
AArch64InstructionEncoding("smullb", ["=z", "z", "z", "zv", "zv"], emit_asm="smullb\t%0.%3, %1.%4, %2.%4")
AArch64InstructionEncoding("smullb", ["=z", "z", "z", "l", "zv", "zv"], emit_asm="smullb\t%0.%4, %1.%5, %2.%5[%3]")
AArch64InstructionEncoding("smullt", ["=z", "z", "z", "zv", "zv"], emit_asm="smullt\t%0.%3, %1.%4, %2.%4")
AArch64InstructionEncoding("smullt", ["=z", "z", "z", "l", "zv", "zv"], emit_asm="smullt\t%0.%4, %1.%5, %2.%5[%3]")
AArch64InstructionEncoding("splice", ["+z", "p_lo", "z", "zv"], emit_asm="splice\t%0.%3, %1, %0.%3, %2.%3")
AArch64InstructionEncoding("sqadd", ["=z", "z", "z", "zv"], emit_asm="sqadd\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("sqadd", ["=q", "q", "q", "qv"], emit_asm="sqadd\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("sqneg", ["=z", "p_lo", "z", "zv"], emit_asm="sqneg\t%0.%3, %1/m, %2.%3")
AArch64InstructionEncoding("sqneg", ["=q", "q", "qv"], emit_asm="sqneg\t%v0.%2, %v1.%2")
AArch64InstructionEncoding("sqrdcmlah", ["+z", "z", "z", "i", "zv"], emit_asm="sqrdcmlah\t%0.%4, %1.%4, %2.%4, %3")
AArch64InstructionEncoding("sqrdmlah", ["+z", "z", "z", "l", "zv"], emit_asm="sqrdmlah\t%0.%4, %1.%4, %2.%4[%3]")
AArch64InstructionEncoding("sqrdmlah", ["+q", "q", "q", "l", "qv"], emit_asm="sqrdmlah\t%v0.%4, %v1.%4, %v2.%l4[%3]")
AArch64InstructionEncoding("sqrdmlah", ["+z", "z", "z", "zv"], emit_asm="sqrdmlah\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("sqrdmlah", ["+q", "q", "q", "qv"], emit_asm="sqrdmlah\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("sqrdmlsh", ["+z", "z", "z", "l", "zv"], emit_asm="sqrdmlsh\t%0.%4, %1.%4, %2.%4[%3]")
AArch64InstructionEncoding("sqrdmlsh", ["+q", "q", "q", "l", "qv"], emit_asm="sqrdmlsh\t%v0.%4, %v1.%4, %v2.%l4[%3]")
AArch64InstructionEncoding("sqrdmlsh", ["+z", "z", "z", "zv"], emit_asm="sqrdmlsh\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("sqrdmlsh", ["+q", "q", "q", "qv"], emit_asm="sqrdmlsh\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("sqrdmulh", ["=z", "z", "z", "l", "zv"], emit_asm="sqrdmulh\t%0.%4, %1.%4, %2.%4[%3]")
AArch64InstructionEncoding("sqrdmulh", ["=q", "q", "q", "l", "qv"], emit_asm="sqrdmulh\t%v0.%4, %v1.%4, %v2.%l4[%3]")
AArch64InstructionEncoding("sqrdmulh", ["=z", "z", "z", "zv"], emit_asm="sqrdmulh\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("sqrdmulh", ["=q", "q", "q", "qv"], emit_asm="sqrdmulh\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("sqrshrn", ["=q", "q", "i", "qv", "qv"], emit_asm="sqrshrn\t%v0.%3, %v1.%4, %2")
AArch64InstructionEncoding("sqrshrn2", ["+q", "q", "i", "qv", "qv"], emit_asm="sqrshrn2\t%v0.%3, %v1.%4, %2")
AArch64InstructionEncoding("sqrshrnb", ["+z", "z", "i", "zv", "zv"], emit_asm="sqrshrnb\t%0.%3, %1.%4, %2")
AArch64InstructionEncoding("sqrshrnt", ["+z", "z", "i", "zv", "zv"], emit_asm="sqrshrnt\t%0.%3, %1.%4, %2")
AArch64InstructionEncoding("sqsub", ["=z", "z", "z", "zv"], emit_asm="sqsub\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("sqsub", ["=q", "q", "q", "qv"], emit_asm="sqsub\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("srshr", ["+z", "p_lo", "i", "zv"], emit_asm="srshr\t%0.%3, %1/m, %0.%3, %2")
AArch64InstructionEncoding("srshr", ["=q", "q", "i", "qv"], emit_asm="srshr\t%v0.%3, %v1.%3, %2")
AArch64InstructionEncoding("st1_q", ["q", "i", "r_sp", "qv"], emit_asm="st1\t{%v0.%l3}[%l1], [%2]")
AArch64InstructionEncoding("st1b", ["z", "p_lo", "r_sp", "i", "zv"], emit_asm="st1b\t{%0.%4}, %1, [%2%,z3]")
AArch64InstructionEncoding("st1b", ["z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="st1b\t{%0.%4}, %1, [%2, %3]")
AArch64InstructionEncoding("st1b", ["z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="st1b\t{%0.%5}, %1, [%2, %3.%5, %4]")
AArch64InstructionEncoding("st1b", ["za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "st1b\t{%0%1%2.b[%w3, %4]}, %5, [%6, %7]",
        "no_offset": "st1b\t{%0%1%2.b[%w3, %4]}, %5, [%6]"})
AArch64InstructionEncoding("st1d", ["z", "p_lo", "r_sp", "i", "zv"], emit_asm="st1d\t{%0.%4}, %1, [%2%,z3]")
AArch64InstructionEncoding("st1d", ["z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="st1d\t{%0.%4}, %1, [%2, %3, lsl #3]")
AArch64InstructionEncoding("st1d", ["z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="st1d\t{%0.%5}, %1, [%2, %3.%5, %4]")
AArch64InstructionEncoding("st1d", ["za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "st1d\t{%0%1%2.d[%w3, %4]}, %5, [%6, %7, lsl #3]",
        "no_offset": "st1d\t{%0%1%2.d[%w3, %4]}, %5, [%6]"})
AArch64InstructionEncoding("st1h", ["z", "p_lo", "r_sp", "i", "zv"], emit_asm="st1h\t{%0.%4}, %1, [%2%,z3]")
AArch64InstructionEncoding("st1h", ["z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="st1h\t{%0.%4}, %1, [%2, %3, lsl #1]")
AArch64InstructionEncoding("st1h", ["z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="st1h\t{%0.%5}, %1, [%2, %3.%5, %s4]")
AArch64InstructionEncoding("st1h", ["za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "st1h\t{%0%1%2.h[%w3, %4]}, %5, [%6, %7, lsl #1]",
        "no_offset": "st1h\t{%0%1%2.h[%w3, %4]}, %5, [%6]"})
AArch64InstructionEncoding("st1q", ["za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "st1q\t{%0%1%2.q[%w3, %4]}, %5, [%6, %7, lsl #4]",
        "no_offset": "st1q\t{%0%1%2.q[%w3, %4]}, %5, [%6]"})
AArch64InstructionEncoding("st1w", ["z", "p_lo", "r_sp", "i", "zv"], emit_asm="st1w\t{%0.%4}, %1, [%2%,z3]")
AArch64InstructionEncoding("st1w", ["z", "p_lo", "r_sp", "r_xzr", "zv"], emit_asm="st1w\t{%0.%4}, %1, [%2, %3, lsl #2]")
AArch64InstructionEncoding("st1w", ["z", "p_lo", "r_sp", "z", "sh", "zv"], emit_asm="st1w\t{%0.%5}, %1, [%2, %3.%5, %s4]")
AArch64InstructionEncoding("st1w", ["za", "l", "hv", "r_12_15", "l", "p_lo", "r_sp", "r_xzr"], emit_asm={
        "with_offset": "st1w\t{%0%1%2.s[%w3, %4]}, %5, [%6, %7, lsl #2]",
        "no_offset": "st1w\t{%0%1%2.s[%w3, %4]}, %5, [%6]"})
AArch64InstructionEncoding("st2b", ["z", "z", "p_lo", "r_sp", "i"], emit_asm="st2b\t{%0.b, %1.b}, %2, [%3%,z4]")
AArch64InstructionEncoding("st2b", ["z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st2b\t{%0.b, %1.b}, %2, [%3, %4]")
AArch64InstructionEncoding("st2h", ["z", "z", "p_lo", "r_sp", "i"], emit_asm="st2h\t{%0.h, %1.h}, %2, [%3%,z4]")
AArch64InstructionEncoding("st2h", ["z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st2h\t{%0.h, %1.h}, %2, [%3, %4, lsl #1]")
AArch64InstructionEncoding("st2w", ["z", "z", "p_lo", "r_sp", "i"], emit_asm="st2w\t{%0.s, %1.s}, %2, [%3%,z4]")
AArch64InstructionEncoding("st2w", ["z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st2w\t{%0.s, %1.s}, %2, [%3, %4, lsl #2]")
AArch64InstructionEncoding("st2d", ["z", "z", "p_lo", "r_sp", "i"], emit_asm="st2d\t{%0.d, %1.d}, %2, [%3%,z4]")
AArch64InstructionEncoding("st2d", ["z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st2d\t{%0.d, %1.d}, %2, [%3, %4, lsl #3]")
AArch64InstructionEncoding("st2q", ["z", "z", "p_lo", "r_sp", "i"], emit_asm="st2q\t{%0.q, %1.q}, %2, [%3%,z4]")
AArch64InstructionEncoding("st2q", ["z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st2q\t{%0.q, %1.q}, %2, [%3, %4, lsl #4]")
AArch64InstructionEncoding("st3b", ["z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st3b\t{%0.b, %1.b, %2.b}, %3, [%4%,z5]")
AArch64InstructionEncoding("st3b", ["z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st3b\t{%0.b, %1.b, %2.b}, %3, [%4, %5]")
AArch64InstructionEncoding("st3h", ["z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st3h\t{%0.h, %1.h, %2.h}, %3, [%4%,z5]")
AArch64InstructionEncoding("st3h", ["z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st3h\t{%0.h, %1.h, %2.h}, %3, [%4, %5, lsl #1]")
AArch64InstructionEncoding("st3w", ["z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st3w\t{%0.s, %1.s, %2.s}, %3, [%4%,z5]")
AArch64InstructionEncoding("st3w", ["z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st3w\t{%0.s, %1.s, %2.s}, %3, [%4, %5, lsl #2]")
AArch64InstructionEncoding("st3d", ["z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st3d\t{%0.d, %1.d, %2.d}, %3, [%4%,z5]")
AArch64InstructionEncoding("st3d", ["z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st3d\t{%0.d, %1.d, %2.d}, %3, [%4, %5, lsl #3]")
AArch64InstructionEncoding("st3q", ["z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st3q\t{%0.q, %1.q, %2.q}, %3, [%4%,z5]")
AArch64InstructionEncoding("st3q", ["z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st3q\t{%0.q, %1.q, %2.q}, %3, [%4, %5, lsl #4]")
AArch64InstructionEncoding("st4b", ["z", "z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st4b\t{%0.b, %1.b, %2.b, %3.b}, %4, [%5%,z6]")
AArch64InstructionEncoding("st4b", ["z", "z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st4b\t{%0.b, %1.b, %2.b, %3.b}, %4, [%5, %6]")
AArch64InstructionEncoding("st4h", ["z", "z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st4h\t{%0.h, %1.h, %2.h, %3.h}, %4, [%5%,z6]")
AArch64InstructionEncoding("st4h", ["z", "z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st4h\t{%0.h, %1.h, %2.h, %3.h}, %4, [%5, %6, lsl #1]")
AArch64InstructionEncoding("st4w", ["z", "z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st4w\t{%0.s, %1.s, %2.s, %3.s}, %4, [%5%,z6]")
AArch64InstructionEncoding("st4w", ["z", "z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st4w\t{%0.s, %1.s, %2.s, %3.s}, %4, [%5, %6, lsl #2]")
AArch64InstructionEncoding("st4d", ["z", "z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st4d\t{%0.d, %1.d, %2.d, %3.d}, %4, [%5%,z6]")
AArch64InstructionEncoding("st4d", ["z", "z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st4d\t{%0.d, %1.d, %2.d, %3.d}, %4, [%5, %6, lsl #3]")
AArch64InstructionEncoding("st4q", ["z", "z", "z", "z", "p_lo", "r_sp", "i"], emit_asm="st4q\t{%0.q, %1.q, %2.q, %3.q}, %4, [%5%,z6]")
AArch64InstructionEncoding("st4q", ["z", "z", "z", "z", "p_lo", "r_sp", "r_xzr"], emit_asm="st4q\t{%0.q, %1.q, %2.q, %3.q}, %4, [%5, %6, lsl #4]")
AArch64InstructionEncoding("stp_x", ["r_xzr", "r_xzr", "r_sp", "i"], emit_asm="stp\t%0, %1, [%2%,3]")
AArch64InstructionEncoding("stp_x_postindex", ["r_xzr", "r_xzr", "+r_sp", "i"], emit_asm="stp\t%0, %1, [%2], %3")
AArch64InstructionEncoding("stp_x_preindex", ["r_xzr", "r_xzr", "+r_sp", "i"], emit_asm="stp\t%0, %1, [%2, %3]!")
AArch64InstructionEncoding("str", ["z", "r_sp", "i"], emit_asm="str\t%0, [%1%,z2]")
AArch64InstructionEncoding("str_b", ["b", "r_sp", "i"], emit_asm="str\t%0, [%1%,2]")
AArch64InstructionEncoding("str_b", ["b", "r_sp", "r_xzr"], emit_asm="str\t%0, [%1, %2]")
AArch64InstructionEncoding("str_d", ["d", "r_sp", "i"], emit_asm="str\t%0, [%1%,2]")
AArch64InstructionEncoding("str_d", ["d", "r_sp", "r_xzr"], emit_asm="str\t%0, [%1, %2, lsl #3]")
AArch64InstructionEncoding("str_h", ["h", "r_sp", "i"], emit_asm="str\t%0, [%1%,2]")
AArch64InstructionEncoding("str_h", ["h", "r_sp", "r_xzr"], emit_asm="str\t%0, [%1, %2, lsl #1]")
AArch64InstructionEncoding("str_q", ["q", "r_sp", "i"], emit_asm="str\t%0, [%1%,2]")
AArch64InstructionEncoding("str_q", ["q", "r_sp", "r_xzr"], emit_asm="str\t%0, [%1, %2, lsl #4]")
AArch64InstructionEncoding("str_s", ["s", "r_sp", "i"], emit_asm="str\t%0, [%1%,2]")
AArch64InstructionEncoding("str_s", ["s", "r_sp", "r_xzr"], emit_asm="str\t%0, [%1, %2, lsl #2]")
AArch64InstructionEncoding("str_x", ["r_xzr", "r_sp", "i"], emit_asm="str\t%0, [%1%,2]")
AArch64InstructionEncoding("str_x", ["r_xzr", "r_sp", "r_xzr"], emit_asm="str\t%0, [%1, %2]")
AArch64InstructionEncoding("strb_x", ["r_xzr", "r_sp", "i"], emit_asm="strb\t%w0, [%1%,2]")
AArch64InstructionEncoding("strb_x", ["r_xzr", "r_sp", "r_xzr"], emit_asm="strb\t%w0, [%1, %2]")
AArch64InstructionEncoding("sub", ["=r_sp", "r_sp", "i"])
AArch64InstructionEncoding("sub", ["=r_xzr", "r_xzr", "r_xzr", "sh"], emit_asm="sub\t%0, %1, %2%,3")
AArch64InstructionEncoding("sub", ["=q", "q", "q", "qv"], emit_asm="sub\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("sub", ["+z", "i", "zv"], emit_asm="sub\t%0.%2, %0.%2, %1")
AArch64InstructionEncoding("sub", ["+z", "p_lo", "z", "zv"], emit_asm="sub\t%0.%3, %1/m, %0.%3, %2.%3")
AArch64InstructionEncoding("sub", ["=z", "z", "z", "zv"], emit_asm="sub\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("subs", ["=r_xzr", "r_sp", "i"], emit_asm={
        "as_subs": "subs\t%0, %1, %2",
        "as_cmp": "cmp\t%1, %2",  # if %0 == xzr
    })
AArch64InstructionEncoding("subs", ["=r_xzr", "r_xzr", "r_xzr"], emit_asm={
        "as_subs": "subs\t%0, %1, %2",
        "as_cmp": "cmp\t%1, %2",  # if %0 == xzr
    })
AArch64InstructionEncoding("trn1", ["=p_all", "p_all", "p_all", "zv"], emit_asm="trn1\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("trn1", ["=q", "q", "q", "qv"], emit_asm="trn1\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("trn1", ["=z", "z", "z", "zv"], emit_asm="trn1\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("trn2", ["=p_all", "p_all", "p_all", "zv"], emit_asm="trn2\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("trn2", ["=q", "q", "q", "qv"], emit_asm="trn2\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("trn2", ["=z", "z", "z", "zv"], emit_asm="trn2\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("udiv", ["=r_xzr", "r_xzr", "r_xzr"])
AArch64InstructionEncoding("uzp1", ["=p_all", "p_all", "p_all", "zv"], emit_asm="uzp1\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("uzp1", ["=q", "q", "q", "qv"], emit_asm="uzp1\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("uzp1", ["=z", "z", "z", "zv"], emit_asm="uzp1\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("uzp2", ["=p_all", "p_all", "p_all", "zv"], emit_asm="uzp2\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("uzp2", ["=q", "q", "q", "qv"], emit_asm="uzp2\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("uzp2", ["=z", "z", "z", "zv"], emit_asm="uzp2\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("whilele", ["=p_all", "r_xzr", "r_xzr", "zv"], emit_asm="whilele\t%0.%3, %1, %2")
AArch64InstructionEncoding("whilelt", ["=p_all", "r_xzr", "r_xzr", "zv"], emit_asm="whilelt\t%0.%3, %1, %2")
AArch64InstructionEncoding("zip1", ["=p_all", "p_all", "p_all", "zv"], emit_asm="zip1\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("zip1", ["=q", "q", "q", "qv"], emit_asm="zip1\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("zip1", ["=z", "z", "z", "zv"], emit_asm="zip1\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("zip2", ["=p_all", "p_all", "p_all", "zv"], emit_asm="zip2\t%0.%3, %1.%3, %2.%3")
AArch64InstructionEncoding("zip2", ["=q", "q", "q", "qv"], emit_asm="zip2\t%v0.%3, %v1.%3, %v2.%3")
AArch64InstructionEncoding("zip2", ["=z", "z", "z", "zv"], emit_asm="zip2\t%0.%3, %1.%3, %2.%3")
# fmt: on
