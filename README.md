# MLIR Sign Analysis

An out-of-tree MLIR data-flow analysis that determines the sign of integer
values. It builds as a plugin for `mlir-opt` and annotates values whose sign is
known.

## Requirements

- CMake 3.20 or newer
- LLVM built with MLIR and loadable plugins enabled
- `llvm-config`, `mlir-opt`, `mlir-translate`, and `clang` on `PATH`

On macOS, Homebrew's LLVM package provides these tools. On Debian or Ubuntu,
install the LLVM and MLIR development packages.

## Use

Build the plugin:

```sh
make
```

The first build configures CMake automatically. Later builds are incremental
and rebuild only the `SignAnalysis` plugin when its sources change.

Run the analysis on the included SQLite source:

```sh
make run
```

To hide facts produced directly by MLIR constant operations:

```sh
make run-no-constants
```

To analyze another LLVM-dialect MLIR file:

```sh
make run INPUT=path/to/input.mlir
```

Values at `top` or `bottom` are omitted because the analysis has no useful
sign fact for them.

Remove compiled files while preserving the CMake configuration:

```sh
make clean
```

## Source layout

- `SignDomain.h` defines the lattice and join operation.
- `SignAnalysis.cpp` defines transfer functions.
- `SignAnalysis.h` connects the domain to MLIR's sparse data-flow framework.
- `Plugin.cpp` registers the `sign-analysis` pass.
- `Annotate.cpp` and `Annotate.h` print analysis results alongside the IR.

The CMake files handle LLVM discovery and platform-specific plugin linking;
normal development should only require the Makefile commands above.

## License

See [LICENSE](LICENSE).
