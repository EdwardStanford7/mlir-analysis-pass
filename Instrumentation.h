//===- Instrumentation.h - Runtime checks for inferred signs -------------===//
#ifndef SIGN_INSTRUMENTATION_H
#define SIGN_INSTRUMENTATION_H

#include "SignDomain.h"
#include "mlir/IR/Value.h"

namespace sign {

// Insert a sign check after the value becomes available, trapping on failure.
// Currently skips i1 values, Top/Bottom, and values outside LLVM functions.
void insert_assertion(Kind kind, mlir::Value value);

} // namespace sign

#endif
