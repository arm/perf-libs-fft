# SloeJIT

SloeJIT is a low-level assembler and register allocator, targeting applications
requiring fast runtime code generation where common compiler optimizations such
as loop vectorization are less applicable. Example applications may be the
generation of specific matrix-multiply or fast-fourier transform kernels, where
instruction selection and loop vectorization is usually explicit in the kernel
being generated rather than something the compiler has any choice over.

## Building

SloeJIT supports being built either as a single or multi-target library. You may
optionally set the `arch` parameter when calling make to specify to only build a
single target architecture rather than all of them (e.g. if you only care about
aarch64).

```
make                # make everything! (still only one libsloejit.a)
make arch=aarch64   # make with only aarch64 support
```
