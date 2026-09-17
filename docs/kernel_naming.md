# FFT kernel naming and parameters

FFT solving is done either by decomposition (for example through factorisation
and application of the Cooley-Tukey algorithm, or Rader's or Bluestein's
algorithms for prime-factor cases), or through direct solving with small "base
case" kernels. This document describes the naming scheme used to identify these
base-case kernels.

## Introduction

FFTs are supported for both integer fixed-point and floating-point datatypes.

Note that not all combinations make sense, and we only generate those
combinations required to cover the functional space for Decimation in Time (DIT)
and Decimation in Frequency (DIF) compositions for complex-complex transforms,
DIT compositions for real-complex transforms and DIF compositions for
complex-real transforms.

For example: `plfft_ab_3_zzztfj_dit_gs` is a double-precision complex-complex
transform of length 3 applying twiddle factors to the input and outputting
element 0 and 1 into Y and elements 2, conjugated, into `YY`. This function is
utilized in the DIT composition for real-complex transforms.

The FFT base kernels may be used to solve any of r2c/c2r/c2c problems.

- c2c: "complex to complex".
- r2c: "real to complex".
- c2r: "complex to real".

## Parameters

The kernels take the following parameters, in order:

- `X` / `XX`: Input pointers, either real or complex arrays.

- `Y` / `YY`: Output pointers, either real or complex arrays.

- `W`: Twiddle factors, always complex twiddle factor arrays. This parameter is
  only present in `t` (twiddle) kernels. The kernel name includes `dit` or `dif`
  to indicate whether the twiddles are applied in a decimation-in-time (`dit`)
  or decimation-in-frequency (`dif`) layout.

- `istride` / `ostride`: The offset (in number of elements, not bytes) between
  each of the `n` elements of an individual FFT in the input (`X`) / output
  (`Y`) arrays.

- `howmany`: The number of independent FFTs being processed at once. This allows
  us to vectorize without cross-lane dependencies and also means we can
  vectorize irrespective of the vector length.

- `idist` / `odist`: The offset (in number of elements, not bytes) between each
  consecutive FFT problem in the `howmany` batch of independent FFTs in the
  input (`X`) / output (`Y`) arrays. If `howmany == 1` then `idist` and `odist`
  are unused since there is nothing to stride to.

Note ACLE kernels are not required to have the same signature as generated
kernels. The SME2 ACLE kernels have different signature because they also
receive arrays of DFT constants as parameters.

## Name mangling

`idist`, `odist`, `istride`, `ostride` and `howmany` are mangled into the kernel
name as:

- `uu`: `idist == 1`, `odist == 1`, contiguous loads and stores over `howmany`.
- `gu`: `idist != 1`, `odist == 1`, contiguous stores over `howmany`.
- `us`: `idist == 1`, `odist != 1`, contiguous loads over `howmany`.
- `gs`: `idist != 1`, `odist != 1`, non-contiguous loads and stores.
- `tu`: `istride == 1`, `idist == n`, `odist == 1`, contiguous loads over `n`,
  contiguous stores over `howmany`.
- `ut`: `idist == 1`, `ostride == 1`, `odist == n`, contiguous loads over
  `howmany` and contiguous stores over `n`.
- `tt`: `istride == 1`, `ostride == 1`, contiguous loads and stores over `n`.
- `uun`: `howmany == 1`, `idist` and `odist` are irrelevant.

Input / output array precision is referred to as:

- `r` / `p`: 8-bit fixed-point only, real-only / complex.
- `h` / `j`: 16-bit fixed-point or floating-point, real-only / complex.
- `s` / `c`: 32-bit floating-point, real-only / complex.
- `d` / `z`: 64-bit floating-point, real-only / complex.

Fixed-point kernels prefix the tuple with `q{fbits}` (e.g. `q15`), where `fbits`
is the number of fractional bits. The width of the type comes from the tuple
letters (`r`/`p` for 8-bit, `h`/`j` for 16-bit), and the integer bits follow
from that width together with `fbits`.

This naming is combined in a tuple of input-intermediate-output, for example:

- `ccs`: 32-bit floating point complex to real.
- `dzz`: 64-bit floating point real to complex.
- `jjj`: 16-bit floating point complex to complex.

Indicating twiddleness, one of:

- `n`: no twiddle factor
- `t`: has twiddle factor

Indicating direction, one of:

- `f`: forwards transform
- `b`: backwards transform

Direction may be omitted for kernels which handle both forwards and backwards by
receiving the DFT constants as a parameter, such as the SME2 ACLE kernels.

Indicating decimation, zero or one of:

- `dit`: input twiddle applied (Decimation in Time)
- `dif`: output twiddle applied (Decimation in Frequency)

Finally a tag for the vector ISA used, one of:

- `neon`
- `asimdhp`
- `sve`
- `sme`
- `sme2`

For twiddle kernels we also include a tag `ab` or `ac`, used to distinguish
whether the same twiddle factors are applied to each independent FFT problem
uniformly (`ac`) or if each FFT problem needs newly-loaded twiddle factors from
the `W` array (`ab`). In the `ac` case this is an optimization since it allows
us to hoist the twiddle factor loading from the inner `howmany` loop. In a
standard 1d c2c decomposition, `ab` kernels will be used for the final stage and
`ac` kernels for all intermediate stages.

Examples:

- `plfft_2_q7_pppnf_uu_neon`: A length 2 kernel, 8-bit fixed-point, c2c,
  non-twiddle, unit strides on input/output (`idist == odist == 1`), using Neon
  vector computation.
- `plfft_ab_3_q15_jjjtf_dit_uu_sve`: A length 3 kernel, 16-bit fixed-point, c2c,
  twiddle (`ab` layout, so fresh twiddles for each `howmany` iteration), unit
  strides on input/output (`idist == odist == 1`), using SVE.
- `plfft_ac_5_ccctf_dit_gs_sve`: A length 5 kernel, 32-bit floating-point, c2c,
  twiddle (`ac` layout, so the same twiddles shared across all `howmany`
  iterations), non-unit strides for both input and output (`idist != 1` and
  `odist != 1`), using SVE.
- `plfft_256_sccnoh_tt_sme2`: A length 256 kernel, 32-bit floating point, r2c,
  non-twiddle, contiguous within each transform (istride == ostride == 1), may
  be either forwards or backwards depending on the constants passed (see kernel
  declaration for signature), using SME2.

There is also a set of modifiers, where zero or one of these can be applied to
kernels relevant to r2c and c2r transforms:

- `ol`: only output first `halflo = (n + 1)/2` elements (r2c).
- `oh`: only output first `halfhi = n/2 + 1` elements (r2c).
- `il`: input first `halflo` elements from `X` and remainder from `XX`,
  conjugated and in reverse order (c2r).
- `ih`: input first `halfhi` elements from `X` and remainder from `XX`,
  conjugated and in reverse order (c2r).
- `j`: output first `halflo` elements into `Y` and remainder into `YY`,
  conjugated and in reverse order (r2c).

Note that even if one of these modifiers is used, the individual kernel itself
might still take complex input and produce complex output if it is used as part
of a larger Cooley-Tukey decomposition (since only the first/last stage of
r2c/c2r transforms respectively actually involve real data).

Functions containing `il` or `ih` have the extra (input) parameter in the
interface, `XX`, which is a pointer to the last element of the array from which
the conjugated, reversed input is to be read. This array is read in reverse
order, and the parameter appears after `X` in the function signature above.

Functions ending with `j` have the extra (output) parameter in the interface,
`YY`, which is a pointer to the last element of the array to which the
conjugated, reversed output is to be written. This array is written in reverse
order, and the parameter appears after `Y` in the function signature above.
