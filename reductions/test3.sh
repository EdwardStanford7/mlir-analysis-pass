#!/bin/sh
set -eu

# These FileCheck lines describe the annotated MLIR that must survive.
# CHECK-LABEL: llvm.func @multiply_is_zero_or_negative(
# CHECK: [[MASKED:%[0-9]+]] = llvm.and [[ARG:%arg[0-9]+]], {{%[a-zA-Z0-9]+}} : i32 // [[MASKED]] is zero-or-positive
# CHECK: [[POSITIVE_A:%[0-9]+]] = llvm.add [[MASKED]], {{%[a-zA-Z0-9]+}} : i32 // [[POSITIVE_A]] is positive
# CHECK: [[NEGATIVE:%[0-9]+]] = llvm.sub {{%[a-zA-Z0-9]+}}, [[POSITIVE_A]] : i32 // [[NEGATIVE]] is negative
# CHECK: [[POSITIVE_B:%[0-9]+]] = llvm.add {{%[a-zA-Z0-9]+}}, {{%[a-zA-Z0-9]+}} : i32 // [[POSITIVE_B]] is positive
# CHECK: [[NONNEGATIVE:%[0-9]+]] = llvm.sdiv [[POSITIVE_A]], [[POSITIVE_B]] : i32 // [[NONNEGATIVE]] is zero-or-positive
# CHECK: [[RESULT:%[0-9]+]] = llvm.mul [[NEGATIVE]], [[NONNEGATIVE]] : i32 // [[RESULT]] is zero-or-negative

if [ "$#" -ne 1 ] || [ ! -f "$1" ]; then
  echo "usage: $0 candidate.ll" >&2
  exit 2
fi

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
candidate_mlir=$(mktemp "${TMPDIR:-/tmp}/sign-reduce-3.XXXXXX")
trap 'rm -f "$candidate_mlir"' EXIT HUP INT TERM

mlir-translate --import-llvm "$1" > "$candidate_mlir" 2>/dev/null || exit 1
BUILD_DIR="${BUILD_DIR:-$repo_dir/build}" \
  "$repo_dir/run.sh" "$candidate_mlir" | FileCheck "$0" 2>/dev/null
