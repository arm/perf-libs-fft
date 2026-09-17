# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

from .gen_lookup import (
    Lookup,
    datatype,
    direction,
    dist_types,
    inout_mods,
    order_kind,
    twiddleness,
)
from .targets import ASIMDHP_TARGET as asimdhp
from .targets import NEON_TARGET as neon
from .targets import SME_TARGET as sme
from .targets import SVE_TARGET as sve

FLT = None


def Q(fbits: int) -> int:
    return fbits


def is_pow_of_two(n: int) -> bool:
    return n > 0 and (n & (n - 1)) == 0


def is_even(n: int) -> bool:
    return n & 1 == 0


def get_config(ns: list[int]) -> list[Lookup]:
    na, ab, ac = order_kind
    r, h, s, d, p, j, c, z = datatype
    n, dit, dif = twiddleness
    f, b = direction
    gu, gs, us, uu, tu, uun = dist_types

    ps = [n for n in ns if is_pow_of_two(n)]
    evens = [n for n in ns if is_even(n)]
    odds = [n for n in ns if not is_even(n)]

    # fmt: off
    c2c = [
        # half-precision
        Lookup(na, ns   , FLT  , j, j, j, n  , (f, b), None, gs , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f, b), None, gu , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f, b), None, us , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f, b), None, tu , [              sme]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f, b), None, uu , [asimdhp, sve, sme]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f, b), None, uun, [asimdhp          ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f, b), None, gs , [asimdhp, sve     ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f, b), None, gu , [asimdhp, sve     ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f, b), None, tu , [              sme]),
        Lookup(ac, ns   , FLT  , j, j, j, dit, (f, b), None, tu , [              sme]),
        Lookup(ac, ns   , FLT  , j, j, j, dit, (f, b), None, uu , [asimdhp, sve, sme]),
        # single-precision
        Lookup(na, ns   , FLT  , c, c, c, n  , (f, b), None, gs , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f, b), None, gu , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f, b), None, us , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f, b), None, tu , [              sme]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f, b), None, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f, b), None, uun, [neon             ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f, b), None, gs , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f, b), None, gu , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f, b), None, tu , [              sme]),
        Lookup(ac, ns   , FLT  , c, c, c, dit, (f, b), None, tu , [              sme]),
        Lookup(ac, ns   , FLT  , c, c, c, dit, (f, b), None, uu , [neon   , sve, sme]),
        # double-precision
        Lookup(na, ns   , FLT  , z, z, z, n  , (f, b), None, gs , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (f, b), None, gu , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (f, b), None, us , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (f, b), None, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (f, b), None, uun, [neon             ]),
        Lookup(ab, ns   , FLT  , z, z, z, dit, (f, b), None, gs , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , z, z, z, dit, (f, b), None, gu , [neon   , sve     ]),
        Lookup(ac, ns   , FLT  , z, z, z, dit, (f, b), None, uu , [neon   , sve, sme]),
        # Q0.7
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f, b), None, gs , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f, b), None, gu , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f, b), None, us , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f, b), None, uu , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f, b), None, uun, [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dit, (f, b), None, gs , [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dit, (f, b), None, gu , [neon             ]),
        Lookup(ac, ps   , Q( 7), p, p, p, dit, (f, b), None, uu , [neon             ]),
        # Q0.15
        Lookup(na, ps   , Q(15), j, j, j, n  , (f, b), None, gs , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (f, b), None, gu , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (f, b), None, us , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (f, b), None, uu , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (f, b), None, uun, [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dit, (f, b), None, gs , [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dit, (f, b), None, gu , [neon             ]),
        Lookup(ac, ps   , Q(15), j, j, j, dit, (f, b), None, uu , [neon             ]),
    ]

    c2r = [
        # half-precision
        Lookup(na, ns   , FLT  , j, j, h, n  , (b,), None         , gs , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , j, j, h, n  , (b,), None         , gu , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , j, j, h, n  , (b,), None         , us , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , j, j, h, n  , (b,), None         , uu , [asimdhp, sve, sme]),
        Lookup(na, ns   , FLT  , j, j, h, n  , (b,), None         , uun, [asimdhp          ]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (b,), inout_mods.ih, uu , [asimdhp, sve, sme]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (b,), inout_mods.ih, uun, [asimdhp          ]),
        Lookup(ab, odds , FLT  , j, j, j, dif, (b,), inout_mods.ih, gs , [asimdhp, sve     ]),
        Lookup(ab, odds , FLT  , j, j, j, dif, (b,), inout_mods.ih, us , [asimdhp, sve     ]),
        Lookup(ab, evens, FLT  , j, j, j, dif, (b,), inout_mods.il, gs , [asimdhp, sve     ]),
        Lookup(ab, evens, FLT  , j, j, j, dif, (b,), inout_mods.il, us , [asimdhp, sve     ]),
        Lookup(ac, odds , FLT  , j, j, j, dif, (b,), inout_mods.ih, uu , [asimdhp, sve, sme]),
        Lookup(ac, evens, FLT  , j, j, j, dif, (b,), inout_mods.il, uu , [asimdhp, sve, sme]),
        # single-precision
        Lookup(na, ns   , FLT  , c, c, s, n  , (b,), None         , gs , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , c, c, s, n  , (b,), None         , gu , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , c, c, s, n  , (b,), None         , us , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , c, c, s, n  , (b,), None         , uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , c, c, s, n  , (b,), None         , uun, [neon             ]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (b,), inout_mods.ih, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (b,), inout_mods.ih, uun, [neon             ]),
        Lookup(ab, odds , FLT  , c, c, c, dif, (b,), inout_mods.ih, gs , [neon   , sve     ]),
        Lookup(ab, odds , FLT  , c, c, c, dif, (b,), inout_mods.ih, us , [neon   , sve     ]),
        Lookup(ab, evens, FLT  , c, c, c, dif, (b,), inout_mods.il, gs , [neon   , sve     ]),
        Lookup(ab, evens, FLT  , c, c, c, dif, (b,), inout_mods.il, us , [neon   , sve     ]),
        Lookup(ac, odds , FLT  , c, c, c, dif, (b,), inout_mods.ih, uu , [neon   , sve, sme]),
        Lookup(ac, evens, FLT  , c, c, c, dif, (b,), inout_mods.il, uu , [neon   , sve, sme]),
        # double-precision
        Lookup(na, ns   , FLT  , z, z, d, n  , (b,), None         , gs , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , z, z, d, n  , (b,), None         , gu , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , z, z, d, n  , (b,), None         , us , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , z, z, d, n  , (b,), None         , uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , z, z, d, n  , (b,), None         , uun, [neon             ]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (b,), inout_mods.ih, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (b,), inout_mods.ih, uun, [neon             ]),
        Lookup(ab, odds , FLT  , z, z, z, dif, (b,), inout_mods.ih, gs , [neon   , sve     ]),
        Lookup(ab, odds , FLT  , z, z, z, dif, (b,), inout_mods.ih, us , [neon   , sve     ]),
        Lookup(ab, evens, FLT  , z, z, z, dif, (b,), inout_mods.il, gs , [neon   , sve     ]),
        Lookup(ab, evens, FLT  , z, z, z, dif, (b,), inout_mods.il, us , [neon   , sve     ]),
        Lookup(ac, odds , FLT  , z, z, z, dif, (b,), inout_mods.ih, uu , [neon   , sve, sme]),
        Lookup(ac, evens, FLT  , z, z, z, dif, (b,), inout_mods.il, uu , [neon   , sve, sme]),
        # Q0.7
        Lookup(na, ps   , Q( 7), p, p, r, n  , (b,), None         , gs , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, r, n  , (b,), None         , gu , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, r, n  , (b,), None         , us , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, r, n  , (b,), None         , uu , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, r, n  , (b,), None         , uun, [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (b,), inout_mods.ih, uu , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (b,), inout_mods.ih, uun, [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dif, (b,), inout_mods.il, gs , [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dif, (b,), inout_mods.il, us , [neon             ]),
        Lookup(ac, ps   , Q( 7), p, p, p, dif, (b,), inout_mods.il, uu , [neon             ]),
        # Q0.15
        Lookup(na, ps   , Q(15), j, j, h, n  , (b,), None         , gs , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, h, n  , (b,), None         , gu , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, h, n  , (b,), None         , us , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, h, n  , (b,), None         , uu , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, h, n  , (b,), None         , uun, [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (b,), inout_mods.ih, uu , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (b,), inout_mods.ih, uun, [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dif, (b,), inout_mods.il, gs , [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dif, (b,), inout_mods.il, us , [neon             ]),
        Lookup(ac, ps   , Q(15), j, j, j, dif, (b,), inout_mods.il, uu , [neon             ]),
    ]

    r2c = [
        # half-precision
        Lookup(na, ns   , FLT  , h, j, j, n  , (f,), inout_mods.oh, gs , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , h, j, j, n  , (f,), inout_mods.oh, gu , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , h, j, j, n  , (f,), inout_mods.oh, us , [asimdhp, sve     ]),
        Lookup(na, ns   , FLT  , h, j, j, n  , (f,), inout_mods.oh, uu , [asimdhp, sve, sme]),
        Lookup(na, ns   , FLT  , h, j, j, n  , (f,), inout_mods.oh, uun, [asimdhp          ]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f,), inout_mods.oh, uu , [asimdhp, sve, sme]),
        Lookup(na, ns   , FLT  , j, j, j, n  , (f,), inout_mods.oh, uun, [asimdhp          ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f,), inout_mods.j , gs , [asimdhp, sve     ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f,), inout_mods.j , gu , [asimdhp, sve     ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f,), inout_mods.j , tu , [              sme]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f,), inout_mods.ol, uun, [asimdhp          ]),
        Lookup(ab, ns   , FLT  , j, j, j, dit, (f,), inout_mods.ol, uu,  [              sme]),
        Lookup(ac, ns   , FLT  , j, j, j, dit, (f,), inout_mods.j , uu , [asimdhp, sve, sme]),
        Lookup(ac, ns   , FLT  , j, j, j, dit, (f,), inout_mods.ol, uu , [asimdhp, sve, sme]),
        # single-precision
        Lookup(na, ns   , FLT  , s, c, c, n  , (f,), inout_mods.oh, gs , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , s, c, c, n  , (f,), inout_mods.oh, gu , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , s, c, c, n  , (f,), inout_mods.oh, us , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , s, c, c, n  , (f,), inout_mods.oh, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , s, c, c, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f,), inout_mods.oh, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , c, c, c, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f,), inout_mods.j , gs , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f,), inout_mods.j , gu , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f,), inout_mods.j , tu , [              sme]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f,), inout_mods.ol, uun, [neon             ]),
        Lookup(ab, ns   , FLT  , c, c, c, dit, (f,), inout_mods.ol, uu,  [              sme]),
        Lookup(ac, ns   , FLT  , c, c, c, dit, (f,), inout_mods.j , uu , [neon   , sve, sme]),
        Lookup(ac, ns   , FLT  , c, c, c, dit, (f,), inout_mods.ol, uu , [neon   , sve, sme]),
        # double-precision
        Lookup(na, ns   , FLT  , d, z, z, n  , (f,), inout_mods.oh, gs , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , d, z, z, n  , (f,), inout_mods.oh, gu , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , d, z, z, n  , (f,), inout_mods.oh, us , [neon   , sve     ]),
        Lookup(na, ns   , FLT  , d, z, z, n  , (f,), inout_mods.oh, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , d, z, z, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (f,), inout_mods.oh, uu , [neon   , sve, sme]),
        Lookup(na, ns   , FLT  , z, z, z, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(ab, ns   , FLT  , z, z, z, dit, (f,), inout_mods.j , gs , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , z, z, z, dit, (f,), inout_mods.j , gu , [neon   , sve     ]),
        Lookup(ab, ns   , FLT  , z, z, z, dit, (f,), inout_mods.ol, uun, [neon             ]),
        Lookup(ab, ns   , FLT  , z, z, z, dit, (f,), inout_mods.ol, uu,  [              sme]),
        Lookup(ac, ns   , FLT  , z, z, z, dit, (f,), inout_mods.j , uu , [neon   , sve, sme]),
        Lookup(ac, ns   , FLT  , z, z, z, dit, (f,), inout_mods.ol, uu , [neon   , sve, sme]),
        # Q0.7
        Lookup(na, ps   , Q( 7), r, p, p, n  , (f,), inout_mods.oh, gs , [neon             ]),
        Lookup(na, ps   , Q( 7), r, p, p, n  , (f,), inout_mods.oh, gu , [neon             ]),
        Lookup(na, ps   , Q( 7), r, p, p, n  , (f,), inout_mods.oh, us , [neon             ]),
        Lookup(na, ps   , Q( 7), r, p, p, n  , (f,), inout_mods.oh, uu , [neon             ]),
        Lookup(na, ps   , Q( 7), r, p, p, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f,), inout_mods.oh, uu , [neon             ]),
        Lookup(na, ps   , Q( 7), p, p, p, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dit, (f,), inout_mods.j , gs , [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dit, (f,), inout_mods.j , gu , [neon             ]),
        Lookup(ab, ps   , Q( 7), p, p, p, dit, (f,), inout_mods.ol, uun, [neon             ]),
        Lookup(ac, ps   , Q( 7), p, p, p, dit, (f,), inout_mods.j , uu , [neon             ]),
        Lookup(ac, ps   , Q( 7), p, p, p, dit, (f,), inout_mods.ol, uu , [neon             ]),
        # Q0.15
        Lookup(na, ps   , Q(15), h, j, j, n  , (f,), inout_mods.oh, gs , [neon             ]),
        Lookup(na, ps   , Q(15), h, j, j, n  , (f,), inout_mods.oh, gu , [neon             ]),
        Lookup(na, ps   , Q(15), h, j, j, n  , (f,), inout_mods.oh, us , [neon             ]),
        Lookup(na, ps   , Q(15), h, j, j, n  , (f,), inout_mods.oh, uu , [neon             ]),
        Lookup(na, ps   , Q(15), h, j, j, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (f,), inout_mods.oh, uu , [neon             ]),
        Lookup(na, ps   , Q(15), j, j, j, n  , (f,), inout_mods.oh, uun, [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dit, (f,), inout_mods.j , gs , [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dit, (f,), inout_mods.j , gu , [neon             ]),
        Lookup(ab, ps   , Q(15), j, j, j, dit, (f,), inout_mods.ol, uun, [neon             ]),
        Lookup(ac, ps   , Q(15), j, j, j, dit, (f,), inout_mods.j , uu , [neon             ]),
        Lookup(ac, ps   , Q(15), j, j, j, dit, (f,), inout_mods.ol, uu , [neon             ]),
    ]
    # fmt: on

    return c2c + c2r + r2c
