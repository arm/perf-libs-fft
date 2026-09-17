# perf-libs-fft

perf-libs-fft (PLFFT) is a high-performance library for one-dimensional Fast
Fourier Transforms (FFTs) on AArch64 processors. It provides a portable C API
and uses modular ahead-of-time (AOT) and just-in-time (JIT) backends to select
optimized kernels at runtime.

## Workflow

Like [FFTW](https://www.fftw.org/), PLFFT creates an executable plan that can be
run repeatedly. PLFFT adds a separate configuration stage before plan creation.
A typical call sequence is:

1. Create a configuration for one transform kind.
2. Set its data type, batching, and alias options.
3. Create a plan from the configuration.
4. Execute the plan as many times as required.
5. Destroy the configuration.
6. Destroy the plan.

For example, an FP32 forward complex transform can be set up as follows:

```c
#include <plfft.h>

plfft_config_t *config = NULL;
plfft_plan_t *plan = NULL;

plfft_status_t status =
    plfft_config_create(&config, n, PLFFT_FORWARD);

if (status == PLFFT_OK)
  status = plfft_plan_create_from_config(&plan, config);

if (status == PLFFT_OK)
  plfft_plan_execute(plan, input, output);

plfft_config_destroy(config);
plfft_plan_destroy(plan);
```

A configuration may be used to create more than one plan. Both destroy functions
accept `NULL`.

## Configuration defaults

All functions that create a configuration install the same defaults:

| Property                         | Default                |
| -------------------------------- | ---------------------- |
| Data type                        | `PLFFT_DATA_TYPE_FP32` |
| I/O alias mode                   | `PLFFT_IO_NO_ALIAS`    |
| SME mode                         | `PLFFT_SME_DISABLED`   |
| Number of transforms (`howmany`) | `1`                    |
| Input and output strides         | `1` element            |
| Input and output batch distances | `n` elements           |

Choose the configuration-creation function for the required transform:

- `plfft_config_create()` creates a complex-to-complex (C2C) transform and takes
  `PLFFT_FORWARD` or `PLFFT_BACKWARD`.
- `plfft_config_create_r2c()` creates a forward real-to-complex (R2C) transform.
- `plfft_config_create_c2r()` creates a backward complex-to-real (C2R)
  transform.
- `plfft_config_create_r2r()` creates a real-to-real (R2R) transform whose
  operation is selected by its transform-kind argument.

Configuration setters only record their values. Enumeration and batch validation
happen during plan creation.

## Data representation

The configured type determines the scalar stored in every input and output
element:

| PLFFT data type        | C storage | Meaning                                       |
| ---------------------- | --------- | --------------------------------------------- |
| `PLFFT_DATA_TYPE_FP16` | `__fp16`  | IEEE binary16                                 |
| `PLFFT_DATA_TYPE_FP32` | `float`   | IEEE binary32                                 |
| `PLFFT_DATA_TYPE_FP64` | `double`  | IEEE binary64                                 |
| `PLFFT_DATA_TYPE_Q7`   | `int8_t`  | signed Q0.7, with real value `stored / 2^7`   |
| `PLFFT_DATA_TYPE_Q15`  | `int16_t` | signed Q0.15, with real value `stored / 2^15` |

Complex buffers contain interleaved real and imaginary scalar values:

```
real[0], imag[0], real[1], imag[1], ....
```

Each logical complex element occupies two consecutive scalars of the configured
data type. Complex strides and distances are measured in complex elements, not
scalar values.

PLFFT does not convert between buffer types and does not carry type information
in `plfft_plan_execute()`. Passing buffers that do not match the planned type
has undefined behavior.

## Fourier transforms and buffer lengths

For floating-point C2C transforms, with `j,k = 0,...,n-1`, PLFFT computes the
unnormalized transforms:

```text
forward:   X[k] = sum_j x[j] exp(-2 pi i j k / n)
backward:  X[k] = sum_j x[j] exp(+2 pi i j k / n)
```

The logical element counts for one transform are:

| Transform | Input                             | Output                            |
| --------- | --------------------------------- | --------------------------------- |
| C2C       | `n` complex elements              | `n` complex elements              |
| R2C       | `n` real elements                 | `floor(n/2) + 1` complex elements |
| C2R       | `floor(n/2) + 1` complex elements | `n` real elements                 |
| R2R       | `n` real elements                 | `n` real elements                 |

R2C returns bins `X[0]` through `X[floor(n/2)]` from the forward definition
above. C2R accepts that non-redundant half-spectrum, reconstructs the omitted
bins by conjugate symmetry, and applies the backward transform.

## Scaling

Floating-point C2C, R2C, and C2R transforms apply no normalization. A C2C
forward transform followed by a backward transform, or an R2C transform followed
by a C2R transform, returns `n*x`. Multiplying the output of either transform in
the pair by `1/n` makes the round trip return `x`.

Q0.7 and Q0.15 transforms instead apply a factor of `1/n` during every transform
to limit overflow. A fixed-point forward/backward round trip therefore returns
approximately `x/n`, subject to quantization and rounding.

R2R transforms also apply no normalization. Their forward/backward or
self-inverse round-trip factors depend on the transform kind and are listed in
the [real-to-real conventions](#real-to-real-conventions) table.

## Batching, strides, and distances

After creating a configuration and before creating a plan, use
`plfft_config_set_batch()` to configure a batch of `howmany` transforms. The
function also sets the strides between logical elements within each transform
and the distances between consecutive transforms. For transform `b`, logical
input element `j`, and logical output element `k`, PLFFT addresses

```text
input [b * idist + j * istride]
output[b * odist + k * ostride]
```

Both input and output are defined in units of their respective element type,
never bytes and never individual real/imaginary components. For example, an R2C
input stride counts real scalars while its output stride counts complex
elements. A C2R input distance similarly counts complex elements while its
output distance counts real scalars.

For positive strides and distances, a buffer holding `howmany` transforms with
`n` logical elements per transform must span at least

```text
(howmany - 1) * dist + (n - 1) * stride + 1
```

elements. Use the input and output element counts from the
[buffer-length table](#fourier-transforms-and-buffer-lengths). When
`howmany == 1`, only positive input and output strides are required and the two
distances are unused. When `howmany > 1`, `howmany`, both strides, and both
distances must all be positive.

The output of each batch member must be disjoint from every other member's input
and output. If a member's own input and output overlap, see
[Aliasing](#aliasing).

## Aliasing

`PLFFT_IO_NO_ALIAS`, the default, selects out-of-place planning and requires
separate input and output storage. `PLFFT_IO_MAY_ALIAS` selects the library's
overlap-safe planning path and permits the input and output buffers of a batch
member to overlap.

When the buffers overlap, the underlying storage must be large enough to satisfy
the buffer-size requirements for both the input and output.

For batched transforms, overlap is only supported within a single member. As
described under [batching](#batching-strides-and-distances), a member's output
must not overlap any other member's input or output.

## Real-to-real conventions

R2R transforms are floating-point only. As with DFTs, all R2R transform
implementations are unnormalized. Hence a transform followed by its inverse will
result in the original data multiplied by a scale factor. The below table maps
PLFFT kind to its transform type and FFTW equivalent, inverse, and the
corresponding round-trip scale factor.

| PLFFT kind        | Convention                              | Inverse kind | Round-trip factor |
| ----------------- | --------------------------------------- | ------------ | ----------------- |
| `PLFFT_R2R_DCT_1` | DCT-I / `FFTW_REDFT00`                  | DCT-I        | `2(n-1)`          |
| `PLFFT_R2R_DCT_2` | DCT-II / `FFTW_REDFT10`                 | DCT-III      | `2n`              |
| `PLFFT_R2R_DCT_3` | DCT-III / `FFTW_REDFT01`                | DCT-II       | `2n`              |
| `PLFFT_R2R_DCT_4` | DCT-IV / `FFTW_REDFT11`                 | DCT-IV       | `2n`              |
| `PLFFT_R2R_DST_1` | DST-I / `FFTW_RODFT00`                  | DST-I        | `2(n+1)`          |
| `PLFFT_R2R_DST_2` | DST-II / `FFTW_RODFT10`                 | DST-III      | `2n`              |
| `PLFFT_R2R_DST_3` | DST-III / `FFTW_RODFT01`                | DST-II       | `2n`              |
| `PLFFT_R2R_DST_4` | DST-IV / `FFTW_RODFT11`                 | DST-IV       | `2n`              |
| `PLFFT_R2R_DHT`   | discrete Hartley transform / `FFTW_DHT` | DHT          | `n`               |
| `PLFFT_R2R_R2HC`  | real DFT to FFTW half-complex form      | HC2R         | `n`               |
| `PLFFT_R2R_HC2R`  | FFTW half-complex form to real DFT      | R2HC         | `n`               |

DCT-I is undefined for `n == 1`; in that case plan creation returns
`PLFFT_PLAN_CREATION_FAILED`.

R2HC and HC2R use FFTW's
[halfcomplex format](https://www.fftw.org/fftw3_doc/The-Halfcomplex_002dformat-DFT.html).

## AOT and JIT kernel providers

PLFFT creates an FFT plan by selecting and composing optimized, low-level FFT
routines called kernels. The user chooses whether to use kernels generated
offline (ahead-of-time, AOT) or at planning time (just-in-time, JIT) when
configuring the library with the CMake option `PLFFT_KERNEL_PROVIDER`:

```bash
cmake -B build -DPLFFT_KERNEL_PROVIDER=aot # Default
cmake -B build -DPLFFT_KERNEL_PROVIDER=jit
```

Each PLFFT build contains exactly one kernel provider. The provider is fixed for
the resulting library and cannot be changed through a PLFFT configuration or
plan.

The AOT backend selects and composes kernels from a fixed inventory compiled
into the library. It does not generate code or allocate executable memory at
runtime.

The JIT backend generates kernels during plan creation, specializing them for
the requested transform and layout. This may produce a plan that executes faster
than an AOT plan, but generating the kernels increases planning time. Generated
kernels are cached process-wide for reuse by later plans. The JIT backend
requires that the process be permitted to allocate executable memory.

Both backends select implementations using CPU features detected during
planning. They use the same PLFFT API, transform definitions, buffer layouts,
and plan lifecycle.

Both providers support FP32 and FP64 C2C, R2C, C2R, and R2R transforms. Their
data-type support differs as follows:

1. FP16 is supported for all four transform kinds. The AOT backend requires
   native CPU FP16 support. When native FP16 is unavailable, the JIT backend
   transparently uses FP32 kernels.

2. Q0.7 and Q0.15 are only supported for:

   1. The AOT backend.
   2. C2C, R2C, and C2R transforms.
   3. Power-of-two sizes.

The default AOT backend is simpler and more portable, has lower planning
overhead, and supports fixed-point transforms. Choose the JIT backend when its
potential execution-speed improvement is worth the additional planning cost and
portability constraints.

## Architectural features

By default, PLFFT builds support for every architectural feature accepted by the
compiler. At runtime, it detects the current CPU and selects only compatible
implementations, allowing one library to run across multiple Arm CPU
generations.

SME, unlike other extensions, is not considered by the planner by default. Set
the plan configuration option `PLFFT_SME_ENABLED` to allow use of SME or SME2
kernels. Note that this option makes SME permitted, not guaranteed.

## Errors, nulls, and ownership

Configuration-creation functions and `plfft_plan_create_from_config()` return a
`plfft_status_t`. If the caller-provided result pointer is not `NULL`, the
function sets its result to `NULL` before validation. The result remains `NULL`
whenever the function returns a status other than `PLFFT_OK`.

`plfft_status_to_string()` returns a non-owning pointer to a static string. Its
result should not be freed.

Only the configuration-creation and plan-creation entry points perform the null
checks described above. Setters, getters, and `plfft_plan_execute()` require
valid, non-`NULL` handles. Execution additionally requires valid, sufficiently
large input and output buffers. It has no status return and performs no public
argument validation.

PLFFT does not expose a recoverable out-of-memory status. In particular, failure
to allocate a configuration prints a fatal error and aborts the process;
allocation failures during planning or execution are likewise not reported
through `plfft_status_t`.

The caller owns every successfully returned handle and must eventually pass it
to its matching destroy function. The destroy functions accept `NULL`, so they
can be called safely after an API failure.

## Thread safety

A plan may be executed concurrently by multiple threads without synchronizing
access to the plan itself. The caller must avoid conflicting access to input and
output buffers and must not destroy the plan until all executions have
completed.

## OpenMP parallelism

PLFFT can parallelize work within a single execution using OpenMP. This behavior
is enabled at build time with `-DPLFFT_ENABLE_OPENMP=ON`, which gives the
library filename an `_mp` suffix. A build without OpenMP does not create OpenMP
worker threads during execution.

The OpenMP build may parallelize eligible individual transforms and batches,
with the available thread count controlled by the OpenMP runtime. Applications
that also execute plans concurrently should configure the runtime appropriately
to avoid oversubscription. In particular, an execution entered from an existing
OpenMP parallel region does not introduce additional parallelism unless nested
OpenMP parallelism is enabled.
