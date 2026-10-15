//===- Instrumentation.cpp - Runtime checks for inferred signs -----------===//
#include "Instrumentation.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Builders.h"

namespace sign {

void insert_assertion(Kind kind, mlir::Value value) {
    // Boolean sign semantics are deferred. Only instrument scalar integers
    // in LLVM function bodies, never global initializer regions.
    auto type = llvm::dyn_cast<mlir::IntegerType>(value.getType());
    mlir::Block* block = value.getParentBlock();
    if (!type || type.getWidth() == 1 || !block ||
        !llvm::isa<mlir::LLVM::LLVMFuncOp>(block->getParentOp()))
        return;
    if (kind == Kind::Top || kind == Kind::Bottom)
        return;
    mlir::OpBuilder builder(value.getContext());

    // Place the new operations immediately after value's definition.
    // For a block argument, this selects the beginning of its block.
    builder.setInsertionPointAfterValue(value);

    mlir::Location location = value.getLoc();

    // Create zero with the same type as value.
    auto zeroOp = mlir::LLVM::ConstantOp::create(builder, location, value.getType(), builder.getIntegerAttr(value.getType(), 0));

    // Comparison predicate
    mlir::LLVM::ICmpPredicate predicate;
    switch (kind) {
    case Kind::Top:
    case Kind::Bottom:
        return;
    case Kind::Zero:
        predicate = mlir::LLVM::ICmpPredicate::eq;
        break;
    case Kind::Negative:
        predicate = mlir::LLVM::ICmpPredicate::slt;
        break;
    case Kind::ZeroOrNeg:
        predicate = mlir::LLVM::ICmpPredicate::sle;
        break;
    case Kind::Positive:
        predicate = mlir::LLVM::ICmpPredicate::sgt;
        break;
    case Kind::ZeroOrPos:
        predicate = mlir::LLVM::ICmpPredicate::sge;
        break;
    }

    mlir::LLVM::ICmpOp compareOp = mlir::LLVM::ICmpOp::create(builder, location, predicate, value, zeroOp.getResult());

    // This is the comparison's SSA result, with type i1.
    mlir::Value condition = compareOp.getResult();

    // Move everything after the comparison into a continuation block.
    // This also moves the original block's terminator.
    mlir::Block* original = compareOp->getBlock();
    mlir::Block* continuation = original->splitBlock(compareOp->getNextNode());

    // Create a separate failure block before the continuation.
    // createBlock also moves the builder's insertion point into it.
    mlir::Block* failure = builder.createBlock(continuation);

    mlir::LLVM::Trap::create(builder, location);
    mlir::LLVM::UnreachableOp::create(builder, location);

    // The original block lost its terminator when we split it.
    // Give it a conditional branch:
    // true -> continue; false -> trap.
    builder.setInsertionPointToEnd(original);
    mlir::LLVM::CondBrOp::create(builder, location, condition, continuation, failure);
}

} // namespace sign
