# MLIR Sign Analysis

Tracks integer signs in LLVM-dialect MLIR. The analysis is in
`SignDomain.h` and `SignAnalysis.cpp`; runtime checks are in `Instrumentation.cpp`.

Requires LLVM with MLIR (`clang`, `mlir-opt`, `mlir-translate`), CMake 3.20+,
and Python 3. Reductions also use `FileCheck` and `llvm-reduce`.

```sh
make                          # build the plugin
make run                      # show useful sign facts for sqlite3.c
make run-all                  # show all sign facts
make run INPUT=example.mlir    # analyze another input
make test                     # instrument and run all test programs
make test TEST=boundary_add    # run one test program
make test TEST='boundary_*'    # run a subset
make check-reductions         # check the reduction examples
make reduce-all               # regenerate reductions
make clean                    # clean compiled output
```

Tests compare baseline and instrumented executions with zero, one, and two
extra arguments. Passing runs print `PASS` with the program and inputs.
Failures print the operation, source location, expected sign, and actual value.
The runner continues after failures and returns nonzero if any fail.
Generated files are kept in `build/instrumentation-tests/` for debugging.
Add executable `tests/*.mlir` programs to extend the suite.

To instrument and run a program manually:

```sh
./run.sh --instrument example.mlir > instrumented.mlir 2> annotations.txt
mlir-translate --mlir-to-llvmir instrumented.mlir -o instrumented.ll
clang instrumented.ll -o instrumented
./instrumented
```

Instrumentation is opt-in; plain `./run.sh example.mlir` prints annotations.
