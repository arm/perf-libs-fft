# Contributing to PLFFT

Contributions should include tests and documentation where appropriate. Build
and test changes on an AArch64 target before sending them for review.

## Development setup

PLFFT requires CMake 3.15 or newer and a C++20-capable compiler. Python 3.10 or
newer is required by the development scripts.

Install the Python dependencies:

```sh
python3 -m pip install -r requirements.txt
```

## Build and test

The default build uses ahead-of-time (AOT) kernels:

```sh
cmake -S . -B build-aot \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLFFT_KERNEL_PROVIDER=aot
cmake --build build-aot --parallel
ctest --test-dir build-aot --output-on-failure
```

Use a separate build directory for the just-in-time (JIT) provider:

```sh
cmake -S . -B build-jit \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLFFT_KERNEL_PROVIDER=jit
cmake --build build-jit --parallel
ctest --test-dir build-jit --output-on-failure
```

Test both providers when a change affects shared planning or execution code.
Feature-specific changes should also be tested with the relevant data types,
layouts, and transform kinds.

## Checks before review

Run the repository checks over the complete change:

```sh
prek run --all-files
```

## Benchmarking changes

For performance-sensitive changes, build the baseline and change with the same
configuration. Run the same cases from each build on the same otherwise-idle
machine. For example, an FP32 comparison for n = 64, 256 and 1024, can be
generated with:

```sh
./build-baseline/bench_driver.py -o before.json -n 64 256 1024 -p f32
./build-change/bench_driver.py -o after.json -n 64 256 1024 -p f32
python3 bench/print_speedups.py before.json after.json
```

The comparison reports `new / old`, so values below 1 are improvements. In a
review with performance-sensitive changes, include CPU architecture, build
toolchain, and representative benchmark results. See the
[benchmark guide](bench/README.md) for more details on running benchmarks.

## Generated AOT kernels

Do not edit files under `src/providers/aot` that carry a generated-file banner.
Regenerate them from a JIT build:

```sh
cmake --build build-jit --target gen_asm_kernels
```

Commit the generated changes with their source change. See the
[kernel generation guide](scripts/gen_asm_kernels/README.md) for the generator
layout, filtering options, and output files.
