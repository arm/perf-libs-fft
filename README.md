# perf-libs-fft (PLFFT)

PLFFT is a high-performance library for one-dimensional Fourier transforms on
AArch64 processors. It provides a plan-based C API backed by ahead-of-time (AOT)
or just-in-time (JIT) optimized kernels.

## Build

PLFFT requires CMake 3.15 or newer, a C++20-capable toolchain, and an AArch64
target.

Build the default AOT configuration and run its tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Optionally select the JIT provider with `-DPLFFT_KERNEL_PROVIDER=jit` or enable
internal OpenMP parallelism with `-DPLFFT_ENABLE_OPENMP=ON`.

## Install and use

Install the library, headers, and CMake package files:

```sh
cmake --install build --prefix "$PWD/install"
```

CMake consumers can use the exported target:

```cmake
find_package(PLFFT CONFIG REQUIRED)
target_link_libraries(my_program PRIVATE PLFFT::plfft)
```

For a non-system installation, configure the consumer with
`-DCMAKE_PREFIX_PATH=/path/to/install`.

## Documentation and examples

The [PLFFT API guide](docs/plfft.md) describes:

- the configuration, planning, and execution workflow
- transform definitions, buffer layouts, and scaling
- batching, strides, aliasing, and thread safety
- AOT, JIT, SME, and OpenMP behavior
- errors and object ownership

The public declarations are in [`include/plfft.h`](include/plfft.h). Complete C
examples for C2C, R2C, C2R, and R2R transforms are in [`examples/`](examples/).

Generate the HTML API reference with:

```sh
doxygen Doxyfile
```

The output starts at `build/docs/html/index.html`.

## Benchmarking

The build produces `build/bench`. Run `build/bench --help` for its options, or
use the generated driver to run multiple cases:

```sh
python3 build/bench_driver.py -n 256 1024 -p f32
```

See the [benchmark guide](bench/README.md) for details. If you do not wish to
build the benchmarks, configure CMake with `-DPLFFT_BUILD_BENCHMARKS=Off`.

## Contributing

If you wish to contribute to the project, see the
[contributor's guide](CONTRIBUTING.md).

## License

PLFFT is available under either the [MIT license](LICENSES/MIT.txt) or the
[Apache License 2.0](LICENSES/Apache-2.0.txt) with the
[LLVM exception](LICENSES/LLVM-exception.txt).
