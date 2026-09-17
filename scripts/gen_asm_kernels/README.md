# gen_asm_kernels

A set of scripts to generate ASM FFT kernels (using WFTA / SloeJIT) and
corresponding lookup tables.

The scripts generate c2c, c2r, and r2c assembly kernels in half-, single-, and
double-precision for transform sizes 2, 3, 4, ..., 20.

For the full list of generated kernels, see `config.py`.

## Requirements

In addition to those in perf-libs-fft/README.md:

- Python 3.10

## Running gen_asm_kernels

The script is driven by the custom target, `gen_asm_kernels`, defined in the
top-level CMakeLists.txt file.

```
$ cd /path/to/perf-libs-fft
$ pip install -r requirements.txt
$ cmake -B build -DPLFFT_KERNEL_PROVIDER=jit
$ cmake --build build --target gen_asm_kernels
[  1%] Creating directories for 'sloejit_build'
...
[ 98%] Linking CXX executable gen_asm
[ 98%] Built target gen_asm
[100%] Generating assembly kernel source files
-- kernels written to: /path/to/perf-libs-fft/src/providers/aot/asm/neon/plfft_ab_jjjn_gs.S
...
[100%] Built target gen_asm_kernels
```

Instead of generating output, the script can instead check if any of the
previously generated assembly files have changed by using the
`check_asm_kernels` target:

```
$ cd /path/to/perf-libs-fft
$ pip install -r requirements.txt
$ cmake -B build -DPLFFT_KERNEL_PROVIDER=jit
$ cmake --build build --target check_asm_kernels
[  1%] Creating directories for 'sloejit_build'
...
...
[ 98%] Linking CXX executable gen_asm
[ 98%] Built target gen_asm
[100%] Checking if assembly kernel source files have changed
-- file has changed: /path/to/perf-libs-fft/src/providers/aot/asm/sve/plfft_ac_jjjt_uu.S
-- file has changed: /path/to/perf-libs-fft/src/providers/aot/asm/sve/plfft_ab_cccn_gs.S
-- auto-generated kernels have changed and need to be updated. Please run gen_asm_kernels.
```

`check_asm_kernels` will fail if any of the assembly files have changed.

## Output

> After running `gen_asm_kernels`, the new output under `src/providers/aot`
> should be committed / pushed to git

The generated files will be written to `src/providers/aot`.

### generated files layout

```
src/providers/aot
├─ asm/
│  ├─ neon/           # neon assembly kernels
│  └─ sve/            # sve assembly kernels
├─ kernels.hpp        # C declarations for asm kernels
├─ kernel_lookup.*pp  # Lookup tables and functions `lookup_fft_func_(n|t)`
└─ kernels.cmake      # CMake fragment for building assembly kernel sources
```

The asm kernels are grouped by arch/order/datatype/twiddleness/dist, e.g.
`asm/neon/plfft_ab_jjjn_gs.S`. More details at docs/kernel_naming.md.

The C declarations for all the kernels are grouped together into one file,
`src/providers/aot/kernels.hpp`.

### Using the output

In the top-level CMakeLists.txt file:

- The kernels.cmake file should be included
- the source files, `kernel_lookup.*pp` and the full set of asm source files
  should be added to the `plfft_object` target sources in (i.e. appended to
  `SRC_CPP_FILES`)

This will make the kernels and `lookup_fft_func_(n|t)` functions available.

> Note: This is already taken care of by the AOT backend.

## gen_asm_kernels source layout

There are two main parts:

- Python scripts and mako templates
- The `gen_asm` executable, linked with `SloeJIT` and the JIT backend

### `gen_asm_kernels` source layout

| file / directory | description                                             |
| ---------------- | ------------------------------------------------------- |
| main.py          | The entry point. Kernels and lookup tables output here  |
| config.py        | The kernels to be output are defined here               |
| templates/       | Used to generate files in the directory tree above      |
| gen_lookup.py    | Helpers used to render mako templates                   |
| gen_asm.cpp      | Main source code for `gen_asm` executable               |
| parse.cpp        | Parser for command-line arguments provided to `gen_asm` |

The `gen_asm` executable generates the kernels, outputting them into the cwd.

main.py calls `gen_asm`, moves the generated kernels into place, and generates
the support code (lookup tables) used by the AOT backend to access the kernels.

## Filtering kernel generation

You can filter generation to a subset of the lookup table by passing filters to
`main.py`. Filters apply at the file level, so options that would only
regenerate some of the kernels within a single file (e.g. `n` or `dir`) are
intentionally omitted.

Examples:

```
$ python -m scripts.gen_asm_kernels.main build/gen_asm src/providers/aot \
    --arch sme --dist tu

$ python -m scripts.gen_asm_kernels.main build/gen_asm src/providers/aot \
    --types ccc --twid dit

$ python -m scripts.gen_asm_kernels.main build/gen_asm src/providers/aot \
    --fbits q7 --order na
```

If `outdir` is omitted, it defaults to `src/providers/aot` relative to
`main.py`. See `--help` for a list of supported filters.

## Lookup Functions

```
using namespace plfft;
```

The lookup functions have signatures of the form:

```
template<
    typename Tx,
    typename Ty,
    order_kind order,
    dist_types dist,
    out_mods = out_mods::om_none,
    in_mods = in_mods::im_none>
fft_func_n_t<Tx, Ty> *
lookup_fft_func_n(int64_t n, plfft_direction_t dir, bool want_sve,
                  bool want_sme);
```

and similar for `fft_func_t_t`, `fft_func_j_t`, `fft_func_in_t`, and
`fft_func_it_t` kernels.
