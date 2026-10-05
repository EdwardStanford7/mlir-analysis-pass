# LLVM reductions

Each reduction consists of an original LLVM IR file (`inputN.ll`), an
interestingness test (`testN.sh`), and the reduced result (`reducedN.mlir`).

The three tests preserve these dataflow chains:

1. an unknown argument is masked to zero, propagated through multiplication,
   and used to prove an equality true;
2. an unknown argument is masked to a nonnegative value and compared with a
   derived negative value, proving a signed greater-than comparison true;
3. an unknown argument is masked to a nonnegative value, then propagated
   through addition, division, subtraction, and multiplication to produce a
   nonpositive result.

Run them with:

```sh
make check-reductions  # confirm all original inputs are interesting
make reduce1           # run one reduction
make reduce-all        # regenerate all three reducedN.mlir files
```

An interestingness test must exit with status `0` when its input is
interesting. Each script converts the candidate to MLIR, runs the analysis,
and passes the annotated result to `FileCheck`.

The `CHECK` comments at the top of each script are not input programs. They
describe the annotated operations that must remain. Names such as `[[ZERO]]`
capture an SSA value and require later operations to use that exact value.
This prevents the reducer from replacing the whole example with an unrelated
constant expression.

`llvm-reduce` writes a temporary `.reducedN.ll`; the Makefile then creates the
required `reducedN.mlir`. The temporary LLVM files are ignored by Git.
