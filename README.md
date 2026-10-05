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

## Reductions

The `reductions` directory contains the three required `llvm-reduce`
examples. Check their starting inputs or regenerate their reduced MLIR with:

```sh
make check-reductions
make reduce-all
```

See [reductions/README.md](reductions/README.md) for what each reduction
preserves.
