# PLFFT benchmark driver

`bench_driver.py` runs the `bench` binary with the cross-product of the
benchmark options that accept multiple values and prints JSON results. The
`--layout` shorthand selects one stride/distance layout for the run; it does not
participate in the cross-product.

From the source tree, run the driver as a Python module from the repository
root:

```sh
python -m bench.bench_driver -n 256 512 -p f32 -H 1 8 -l tt
```

Use `--transform-kind` (or `-k`) to benchmark complex-to-complex,
real-to-complex, and complex-to-real transforms:

```sh
python -m bench.bench_driver ./build/bench -n 256 -k c2c r2c c2r
```

Direction defaults to forward for C2C, is inferred as forward for R2C, and is
inferred as backward for C2R. The contiguous distance on a Hermitian-complex
side defaults to `n / 2 + 1`; the real and C2C distances default to `n`.

From a build directory, CMake generates a `bench_driver.py` launcher next to the
`bench` binary, so native benchmarks can be run as:

```sh
./bench_driver.py -n 256 512 -p f32 -H 1 8 -l tt
```

The benchmark binary argument is optional. Both invocation types have default
bench binary adjacent to the respective runner script. Passing a path to the
built binary will almost certainly be necessary for source-tree invocation.
Explicit relative paths are interpreted relative to the current working
directory:

```sh
./bench_driver.py ./bench -n 256 -p f32
python -m bench.bench_driver ./build/bench -n 256 -p f32
```

Use `--verbose` to print the exact benchmark binary invocations to stdout before
running them:

```sh
python bench_driver.py -n 256 -p f32 --verbose
```

Use `-t` or `--table` to print the same fields as a compact text table:

```sh
python bench_driver.py -t -n 256 512 -p f32 -H 1 8
```

Use `-o` or `--output` to write the JSON or table output to a file:

```sh
python bench_driver.py -o results.json -n 256 512 -p f32
```

Use `print_speedups.py` to compare two JSON result files:

```sh
python /path/to/perf-libs-fft/bench/print_speedups.py old.json new.json
```

## Runners

The upstream driver has built-in `native` and `adb` runners. The `adb` runner
pushes the benchmark binary to `/data/local/tmp/plfft_bench` and runs it through
`adb shell`:

```sh
python bench_driver.py --runner adb -n 256 -p f32
```

The default destination is `/data/local/tmp/plfft_bench`. To use a different
device directory, pass it as the runner configuration. For example, devices that
only provide `/tmp` can be run with:

```sh
python bench_driver.py --runner adb:/tmp -n 256 -p f32
```

Other execution environments can be added by placing a file named
`bench_runner_hook.py` next to `bench_driver.py`. The hook file must define a
`register` function:

```python
from pathlib import Path

from bench.benchmark_types import AbstractRunner, BenchmarkCase, BenchmarkResult


class MyRunner(AbstractRunner):
    def __init__(self, bench_binary: Path, verbose: bool, config: str | None):
        self.bench_binary = bench_binary
        self.verbose = verbose
        self.config = config

    @classmethod
    def create(cls, bench_binary: Path, verbose: bool, arg: str | None) -> "MyRunner":
        return cls(bench_binary, verbose, arg)

    def execute(self, case: BenchmarkCase) -> BenchmarkResult:
        cycles = run_model(case, self.config)
        return BenchmarkResult(**vars(case), value=cycles, unit="cycles")


def register(register_runner):
    register_runner("my-runner", MyRunner)
```

Then run:

```sh
./bench_driver.py --runner my-runner:default -n 256 -p f32
```

Runners implement `create` and `execute`. `create` receives the benchmark binary
path, verbose flag and the optional part after `:` in `--runner`. `execute`
accepts a benchmark case and returns a benchmark result.
