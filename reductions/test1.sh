#!/bin/sh
set -eu

# These FileCheck lines describe the annotated MLIR that must survive.
# CHECK-LABEL: llvm.func @equal_is_positive(
# CHECK: [[ZERO:%[0-9]+]] = llvm.and [[ARG:%arg[0-9]+]], {{%[a-zA-Z0-9]+}} : i32 // [[ZERO]] is zero
# CHECK: [[ZERO_AGAIN:%[0-9]+]] = llvm.mul [[ZERO]], {{%[a-zA-Z0-9]+}} : i32 // [[ZERO_AGAIN]] is zero
# CHECK: [[RESULT:%[0-9]+]] = llvm.icmp "eq" [[ZERO]], [[ZERO_AGAIN]] : i32 // [[RESULT]] is positive

if [ "$#" -ne 1 ] || [ ! -f "$1" ]; then
  echo "usage: $0 candidate.ll" >&2
  exit 2
fi

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
candidate_mlir=$(mktemp "${TMPDIR:-/tmp}/sign-reduce-1.XXXXXX")
trap 'rm -f "$candidate_mlir"' EXIT HUP INT TERM

mlir-translate --import-llvm "$1" > "$candidate_mlir" 2>/dev/null || exit 1
BUILD_DIR="${BUILD_DIR:-$repo_dir/build}" \
  "$repo_dir/run.sh" "$candidate_mlir" | FileCheck "$0" 2>/dev/null
