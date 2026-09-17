//===- SignAnalysis.cpp - Transfer functions ------------------------------===//
//
// The transfer function: given what is known about an operation's operands,
// state what is known about its results.  This file and SignDomain.h are the
// two to replace when building a different analysis; the rest of the project
// is scaffolding.
//
//===----------------------------------------------------------------------===//

#include "SignAnalysis.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Matchers.h"

using namespace mlir;

namespace sign {

namespace {

// Rows and columns are ordered as: bottom, zero, negative, positive, top.
// Keeping the tables in the same form as the specification makes each
// transfer function easy to check against it.
constexpr unsigned tableIndex(Kind kind) {
    switch (kind) {
    case Kind::Bottom:
        return 0;
    case Kind::Zero:
        return 1;
    case Kind::Negative:
        return 2;
    case Kind::Positive:
        return 3;
    case Kind::Top:
        return 4;
    }
    return 4;
}

using TransferTable = Kind[5][5];

constexpr TransferTable addTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom},
    {Kind::Bottom, Kind::Zero, Kind::Negative, Kind::Positive, Kind::Top},
    {Kind::Bottom, Kind::Negative, Kind::Negative, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Positive, Kind::Top, Kind::Positive, Kind::Top},
    {Kind::Bottom, Kind::Top, Kind::Top, Kind::Top, Kind::Top},
};

constexpr TransferTable subTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom},
    {Kind::Bottom, Kind::Zero, Kind::Positive, Kind::Negative, Kind::Top},
    {Kind::Bottom, Kind::Negative, Kind::Top, Kind::Negative, Kind::Top},
    {Kind::Bottom, Kind::Positive, Kind::Positive, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Top, Kind::Top, Kind::Top, Kind::Top},
};

constexpr TransferTable mulTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom},
    {Kind::Bottom, Kind::Zero, Kind::Zero, Kind::Zero, Kind::Zero},
    {Kind::Bottom, Kind::Zero, Kind::Positive, Kind::Negative, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Negative, Kind::Positive, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Top, Kind::Top, Kind::Top},
};

constexpr TransferTable divTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom}, {Kind::Bottom, Kind::Bottom, Kind::Zero, Kind::Zero, Kind::Zero},
    {Kind::Bottom, Kind::Bottom, Kind::Top, Kind::Top, Kind::Top},          {Kind::Bottom, Kind::Bottom, Kind::Top, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Bottom, Kind::Top, Kind::Top, Kind::Top},
};

constexpr TransferTable greaterThanTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom},
    {Kind::Bottom, Kind::Zero, Kind::Positive, Kind::Zero, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Top, Kind::Zero, Kind::Top},
    {Kind::Bottom, Kind::Positive, Kind::Positive, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Top, Kind::Top, Kind::Top, Kind::Top},
};

constexpr TransferTable equalTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom},
    {Kind::Bottom, Kind::Positive, Kind::Zero, Kind::Zero, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Top, Kind::Zero, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Zero, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Top, Kind::Top, Kind::Top, Kind::Top},
};

constexpr TransferTable andTable = {
    {Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom, Kind::Bottom},
    {Kind::Bottom, Kind::Zero, Kind::Zero, Kind::Zero, Kind::Zero},
    {Kind::Bottom, Kind::Zero, Kind::Top, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Top, Kind::Top, Kind::Top},
    {Kind::Bottom, Kind::Zero, Kind::Top, Kind::Top, Kind::Top},
};
// Rows and columns are ordered as: bottom, zero, negative, positive, top.

SignState evaluate(const TransferTable& table, SignState lhs, SignState rhs) { return table[tableIndex(lhs.kind)][tableIndex(rhs.kind)]; }

} // namespace

void SignAnalysis::setToEntryState(SignLattice* lattice) { propagateIfChanged(lattice, lattice->join(SignState::top())); }

LogicalResult SignAnalysis::visitOperation(Operation* op, ArrayRef<const SignLattice*> operands, ArrayRef<SignLattice*> results) {
    // Raising a result to top says "this operation could produce anything",
    // which is always a sound answer and is what every unhandled case does.
    auto unknown = [&] {
        setAllToEntryStates(results);
        return success();
    };

    // Only single-result integer operations are interesting here.  Calls, loads,
    // floats, and vectors all land in `unknown`.
    if (op->getNumResults() != 1 || !op->getResult(0).getType().isIntOrIndex())
        return unknown();
    SignLattice* result = results[0];

    // Constants seed the analysis with zero, negative, or positive.
    // This is the only rule that does not consult its operands, and without some
    // rule of this kind the analysis would have no facts to propagate at all.
    IntegerAttr value;
    if (matchPattern(op, m_Constant(&value))) {
        SignState state;
        if (value.getValue().isZero()) {
            state = Kind::Zero;
        }
        // else if (value.getValue().isOne()) {
        //     state = Kind::One;
        // }
        else if (value.getValue().isStrictlyPositive()) {
            state = Kind::Positive;
        } else if (value.getValue().isNegative()) {
            state = Kind::Negative;
        }

        propagateIfChanged(result, result->join(state));
        return success();
    }

    if (operands.size() != 2)
        return unknown();

    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();
    auto transfer = [&](const TransferTable& table) {
        propagateIfChanged(result, result->join(evaluate(table, lhs, rhs)));
        return success();
    };

    if (isa<LLVM::AddOp>(op)) {
        return transfer(addTable);
    }
    if (isa<LLVM::SubOp>(op)) {
        return transfer(subTable);
    }
    if (isa<LLVM::MulOp>(op)) {
        return transfer(mulTable);
    }
    if (isa<LLVM::SDivOp>(op)) {
        return transfer(divTable);
    }
    if (isa<LLVM::AndOp>(op)) {
        return transfer(andTable);
    }

    if (auto compare = dyn_cast<LLVM::ICmpOp>(op)) {
        if (compare.getPredicate() == LLVM::ICmpPredicate::sgt) {
            return transfer(greaterThanTable);
        }
        if (compare.getPredicate() == LLVM::ICmpPredicate::eq) {
            return transfer(equalTable);
        }
    }

    return unknown();
}

} // namespace sign
