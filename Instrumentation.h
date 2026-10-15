//===- Instrumentation.h - Runtime checks for inferred signs -------------===//
#ifndef SIGN_INSTRUMENTATION_H
#define SIGN_INSTRUMENTATION_H

#include "SignDomain.h"
#include "mlir/IR/Value.h"

namespace sign {

// Insert a sign check after the value becomes available, reporting and trapping
// on failure (diagnostics use libc's dprintf).
// Currently skips i1 values, Top/Bottom, and values outside LLVM functions.
void insert_assertion(Kind kind, mlir::Value value);

} // namespace sign

#endif
