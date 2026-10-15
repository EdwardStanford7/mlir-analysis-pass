# MLIR Sign Analysis

An MLIR data-flow analysis that tracks whether integer values are zero,
negative, positive, nonpositive, nonnegative, or unknown.

## Requirements

Install LLVM with MLIR support and put these tools on `PATH`:

- `clang`
- `FileCheck`
- `llvm-reduce`
- `mlir-opt`
- `mlir-translate`

CMake 3.20 or newer is also required. Homebrew's `llvm` package provides the
LLVM and MLIR tools on macOS.

## Commands

```sh
make                 # configure once, then build the plugin incrementally
make run             # analyze sqlite3.c and show useful non-constant facts
make run-all         # include constants and uninformative comparison facts
make clean           # remove compiled output but keep CMake configured
```

Use a different LLVM-dialect MLIR input with:

```sh
make run INPUT=path/to/input.mlir
```

The main implementation is in `SignDomain.h` and `SignAnalysis.cpp`.

## Runtime sign checks

Analysis leaves the module unchanged by default. Enable instrumentation with
`--instrument`; this writes a complete instrumented module to stdout and the
original annotated listing to stderr:

```sh
./run.sh --instrument input.mlir > instrumented.mlir 2> annotations.txt
mlir-translate --mlir-to-llvmir instrumented.mlir -o instrumented.ll
clang instrumented.ll -o instrumented
./instrumented
```

The input must contain a `main` function to link as an executable; SQLite's
library amalgamation needs a separate driver or shell. Add any required
libraries when linking.

Checks compare inferred signs against runtime values and trap on failure.
They currently cover scalar integers wider than one bit in LLVM functions;
`i1` values and global initializers are skipped. The implementation is in
`Instrumentation.cpp`.

When invoking `mlir-opt` directly, enable the pass option with
`--pass-pipeline='builtin.module(sign-analysis{instrument=true})'`.
Without that option, `./run.sh input.mlir`, `make run`, and `make run-all`
continue to produce annotations only.

## Reductions

The `reductions` directory contains the three required `llvm-reduce`
examples. Check their starting inputs or regenerate their reduced MLIR with:

```sh
make check-reductions
make reduce-all
```

See [reductions/README.md](reductions/README.md) for what each reduction
preserves.
