# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

import itertools

from dataclasses import astuple, dataclass
from enum import Enum
from functools import cached_property
from typing import cast

from .targets import SME_TARGET, KernelTarget

PREFIX = "plfft"


# fmt: off
class StrEnum(Enum):
    def __str__(self) -> str:
        return self.name


class order_kind(StrEnum):
    na = "ORDER_NA"
    ab = "ORDER_AB"
    ac = "ORDER_AC"


# Represents the data type of the input/output/twiddle factors.
# For fixed-point kernels, this the underlying type and the number of
# fractional bits is specified separately (see `fbits` in `Lookup`).
class datatype(StrEnum):
    r = (None    , "int8_t" )
    h = ("half"  , "int16_t")
    s = ("float" , "int32_t")
    d = ("double", "int64_t")
    p = (None                  , "std::complex<int8_t>" )
    j = ("std::complex<half>"  , "std::complex<int16_t>")
    c = ("std::complex<float>" , "std::complex<int32_t>")
    z = ("std::complex<double>", "std::complex<int64_t>")

class twiddleness(StrEnum):
    n   = "n"
    dit = "dit"
    dif = "dif"


class direction(StrEnum):
    f = "forward"
    b = "backward"

    @property
    def plfft_value(self) -> str:
        return "PLFFT_FORWARD" if self == direction.f else "PLFFT_BACKWARD"


class inout_mods(StrEnum):
    ol = ("halflo"      , "im_none"  )
    oh = ("halfhi"      , "im_none"  )
    il = ("om_none"     , "im_halflo")
    ih = ("om_none"     , "im_halfhi")
    j  = ("conj_reverse", "im_none"  )


class dist_types(StrEnum):
    gu  = "gu"
    gs  = "gs"
    us  = "us"
    uu  = "uu"
    tu  = "tu"
    uun = "uun"
#fmt : on


@dataclass
class Kernel:
    name: str
    fft_func_t: str
    uses_za: bool


@dataclass
class Table:
    name: str
    dir: direction
    fft_func_t: str
    target: KernelTarget
    kernels: dict[int, Kernel]

@dataclass
class Lookup:
    order: order_kind
    ns: list[int]
    fbits: int | None
    tx: datatype
    tw: datatype
    ty: datatype
    twid: twiddleness
    dirs: tuple[direction] | tuple[direction, direction]
    mod: inout_mods | None
    dist: dist_types
    targets: list[KernelTarget]

    @property
    def name(self) -> str:
        order, _, fbits, tx, tw, ty, twid, _, mod, dist, _ = astuple(self)
        ctx, ctw, cty = cpptype(tx, fbits), cpptype(tw, fbits), cpptype(ty, fbits)
        out_mod, in_mod = mod.value if mod else ("om_none", "im_none")
        out_mod = "om_real" if self.is_c2r else out_mod
        in_mod = "im_real" if self.is_r2c else in_mod

        params = (
            f"order_kind::{order.value}, "
            f"dist_types::{dist.value}, "
            f"out_mods::{out_mod}, "
            f"in_mods::{in_mod}"
        )
        if twid == twiddleness.n and mod == inout_mods.ih:
            return f"lookup_fft_func_in<{ctw}, {params}>"
        elif twid == twiddleness.dif:
            return f"lookup_fft_func_it<{ctw}, {params}>"
        elif mod == inout_mods.j:
            return f"lookup_fft_func_j<{ctw}, {params}>"
        elif twid == twiddleness.n:
            return f"lookup_fft_func_n<{ctx}, {cty}, {params}>"
        else:
            assert twid == twiddleness.dit
            return f"lookup_fft_func_t<{ctw}, {params}>"

    @property
    def src(self) -> str:
        order, _, fbits, tx, tw, ty, twid, _, mod, dist, _ = astuple(self)
        name = key(order, None, fbits, tx, tw, ty, twid, None, mod, dist, None)
        return f"{name}.S"

    @property
    def fft_func_t(self) -> str:
        _, _, fbits, tx, tw, ty, twid, _, mod, _, _ = astuple(self)
        ctx, ctw, cty = cpptype(tx, fbits), cpptype(tw, fbits), cpptype(ty, fbits)

        if twid == twiddleness.n and mod == inout_mods.ih:
            return f"fft_func_in_t<{ctw}>"
        elif twid == twiddleness.dif:
            return f"fft_func_it_t<{ctw}>"
        elif self.mod == inout_mods.j:
            return f"fft_func_j_t<{ctw}>"
        if self.twid == twiddleness.n:
            return f"fft_func_n_t<{ctx}, {cty}>"
        else:
            assert twid == twiddleness.dit
            return f"fft_func_t_t<{ctw}>"

    @cached_property
    def tables(self) -> list[Table]:
        order, ns, fbits, tx, tw, ty, twid, dirs, mod, dist, _ = astuple(self)
        output = []
        for dir, target in itertools.product(dirs, self.targets):
            kernels = {}
            for n in ns:
                name = key(
                    order, n, fbits, tx, tw, ty, twid, dir, mod, dist, target
                )
                kernels[n] = Kernel(
                    name, self.fft_func_t, self.uses_za(n, target == SME_TARGET)
                )

            name = key(
                order, None, fbits, tx, tw, ty, twid, dir, mod, dist, target
            )
            output.append(Table(name, dir, self.fft_func_t, target, kernels))

        return output

    def tables_for_targets(self, *targets: KernelTarget) -> list[Table]:
        return [table for table in self.tables if table.target in targets]

    @property
    def is_c2r(self) -> bool:
        return self.ty in (datatype.r, datatype.h, datatype.s, datatype.d)

    @property
    def is_r2c(self) -> bool:
        return self.tx in (datatype.r, datatype.h, datatype.s, datatype.d)

    @property
    def is_fp16(self) -> bool:
        return not self.is_fixed_point and self.tw in (datatype.h, datatype.j)

    @property
    def is_fp32(self) -> bool:
        return not self.is_fixed_point and self.tw in (datatype.s, datatype.c)

    @property
    def is_fp64(self) -> bool:
        return not self.is_fixed_point and self.tw in (datatype.d, datatype.z)

    @property
    def is_fixed_point(self) -> bool:
        return self.fbits is not None

    def uses_za(self, n: int, is_sme: bool) -> bool:
        # This function is used to decide whether to emit a
        # za-preserving wrapper around kernel calls. It needs to be
        # kept up-to-date with shortcuts or deoptimizations in
        # build_input_values_transposing_load to avoid corrupting za
        if not is_sme or self.dist != dist_types.tu:
            return False

        if self.is_fixed_point or self.is_fp64:
            # No fixed-point or FP64 TU kernels currently -
            # conservatively predict that they will all have to use ZA
            return True

        if self.is_fp16:
            non_za_ns = {2, 3, 4, 6, 8}
        elif self.is_fp32:
            non_za_ns = {2, 3, 4}
        else:
            raise ValueError(f"Cannot determine whether kernel {self} uses za")

        return n not in non_za_ns

    @property
    def wrapper_params(self) -> str:
        _, _, fbits, tx, tw, ty, twid, _, mod, _, _ = astuple(self)
        ctx, ctw, cty = cpptype(tx, fbits), cpptype(tw, fbits), cpptype(ty, fbits)

        if twid == twiddleness.n and mod == inout_mods.ih:
            return (
                f"const {ctw} *X, const {ctw} *XX, {ctw} *Y, "
                "int64_t istride, int64_t ostride, int64_t howmany, "
                "int64_t idist, int64_t odist"
            )
        elif twid == twiddleness.dif:
            return (
                f"const {ctw} *X, const {ctw} *XX, {ctw} *Y, "
                "int64_t istride, int64_t ostride, const void *W, "
                "int64_t howmany, int64_t idist, int64_t odist"
            )
        elif mod == inout_mods.j:
            return (
                f"const {ctw} *X, {ctw} *Y, {ctw} *YY, "
                "int64_t istride, int64_t ostride, const void *W, "
                "int64_t howmany, int64_t idist, int64_t odist"
            )
        elif twid == twiddleness.n:
            return (
                f"const {ctx} *X, {cty} *Y, "
                "int64_t istride, int64_t ostride, int64_t howmany, "
                "int64_t idist, int64_t odist"
            )
        else:
            return (
                f"const {ctw} *X, {ctw} *Y, "
                "int64_t istride, int64_t ostride, const void *W, "
                "int64_t howmany, int64_t idist, int64_t odist"
            )

    @property
    def wrapper_args(self) -> str:
        _, _, _, _, _, _, twid, _, mod, _, _ = astuple(self)

        if twid == twiddleness.n and mod == inout_mods.ih:
            return "X, XX, Y, istride, ostride, howmany, idist, odist"
        elif twid == twiddleness.dif:
            return "X, XX, Y, istride, ostride, W, howmany, idist, odist"
        elif mod == inout_mods.j:
            return "X, Y, YY, istride, ostride, W, howmany, idist, odist"
        elif twid == twiddleness.n:
            return "X, Y, istride, ostride, howmany, idist, odist"
        else:
            return "X, Y, istride, ostride, W, howmany, idist, odist"


def cpptype(dt: datatype, fbits: int | None) -> str:
    return cast(str, dt.value[0 if fbits is None else 1])


def key(
    order: order_kind,
    n: int | None,
    fbits: int | None,
    tx: datatype,
    tw: datatype,
    ty: datatype,
    twid: twiddleness,
    dir: direction | None,
    mod: inout_mods | None,
    dist: dist_types,
    target: KernelTarget | None,
) -> str:
    # use to generate filenames, kernel names, and lookup table names see
    # docs/kernel_naming.md.

    output = [PREFIX]

    if order != order_kind.na:
        output.append(str(order))
    if n is not None:
        output.append(str(n))
    if fbits is not None:
        output.append(f"q{fbits}")

    mid = [f"{tx}{tw}{ty}"]
    mid.append("n" if twid == twiddleness.n else "t")
    if dir is not None:
        mid.append(str(dir))
    if mod is not None:
        mid.append(str(mod))
    output.append("".join(mid))

    if twid != twiddleness.n:
        output.append(str(twid))
    output.append(str(dist))

    # target is already mangled into the directory name, so ignored for filename
    if target is not None:
        output.append(target.name)

    return "_".join(output)
