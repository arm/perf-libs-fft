# Overview of AArch64 Registers

## Neon

In AArch64, the floating-point (FP), Neon, (and later SVE) instructions all
operate on a unified 32-entry vector register file. Each register is 128 bits
wide architecturally in Neon.

The registers are aliased into smaller scalar views used by floating-point
operations:

- v0-v31 (128 bits) (also used as q0-q31)
- d0–d31 (64 bits)
- s0–s31 (32 bits)
- h0–h31 (16 bits)
- b0–b31 (8 bits)

These are not separate registers. For example:

- d0 is the low 64 bits of v0/q0.
- s0 is the low 32 bits of d0/v0/q0.

Writing to a register zeros the top part of the register:

- Writing s0 zeroes everything above the low 32 bits of the register.
- Writing d0 zeroes everything above the low 64 bits of the register.
- Writing q0 zeroes everything above the low 128 bits of the register.

## SVE

The Scalable Vector Extension (SVE) extends the existing register file further:

- z0–z31: Vector registers of implementation-defined length (a power of two
  length (VL bits) between 128 and 2048 inclusive). The low 128 bits of these
  registers overlap with the Neon registers.

- p0–p15: Predicate registers of VL/8 bits each, one bit per vector byte. For
  example for VL=256 bits, each predicate is 32 bits or 4 bytes.

- ffr: The First-Fault register.

For example, in a system with 256-bit SVE registers the low 128 bits of z0 alias
the Neon v0 register. Any writes to Neon registers (e.g. s0/d0/v0/q0) clear the
top part of the corresponding SVE register as well.

Any Neon instruction (e.g., fadd v0.4s, v0.4s, v1.4s) touches only the bottom
128 bits and zeros the SVE-extended portion.

This layered aliasing allows software to use Neon and SVE instructions
interchangeably without needing explicit transitions between architectural
modes.
