#!/bin/sh
# Run the analysis over an MLIR file.
#
#   ./run.sh input.mlir
#   ./run.sh --instrument input.mlir > instrumented.mlir
#
# The plugin is SignAnalysis.dylib on macOS and SignAnalysis.so on Linux and
# WSL2, so probe for it rather than hard-coding a suffix.  Set PLUGIN or
# BUILD_DIR to override.
set -eu

usage() {
  echo "usage: $0 [--instrument] input.mlir [mlir-opt options]"
  echo "Default: annotated listing on stdout."
  echo "--instrument: instrumented MLIR on stdout, original annotations on stderr."
}

instrument=false
case "${1:-}" in
  --instrument) instrument=true; shift ;;
  -h|--help) usage; exit 0 ;;
esac

if [ "$#" -lt 1 ]; then
  usage >&2
  exit 2
fi

BUILD_DIR="${BUILD_DIR:-build}"

if [ -z "${PLUGIN:-}" ]; then
  for candidate in "$BUILD_DIR"/SignAnalysis.so "$BUILD_DIR"/SignAnalysis.dylib; do
    if [ -f "$candidate" ]; then
      PLUGIN="$candidate"
      break
    fi
  done
fi

if [ -z "${PLUGIN:-}" ]; then
  echo "No plugin found in $BUILD_DIR; build it first (see README.md)." >&2
  exit 1
fi

if [ "$instrument" = true ]; then
  # Preserve stdout as a complete module suitable for LLVM IR export.
  exec mlir-opt --load-pass-plugin="$PLUGIN" \
    --pass-pipeline='builtin.module(sign-analysis{instrument=true})' "$@"
fi

# stdout is the unchanged IR and stderr is the annotated listing; send the
# listing to this script's stdout so it can be piped or paged.
exec mlir-opt --load-pass-plugin="$PLUGIN" \
         --pass-pipeline='builtin.module(sign-analysis)' \
         "$@" 2>&1 1>/dev/null
