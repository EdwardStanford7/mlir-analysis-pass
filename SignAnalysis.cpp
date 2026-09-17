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

// Rows and columns are ordered as: bottom, zero, negative, zero-or-negative,
// one, positive, zero-or-positive, top.
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
    case Kind::ZeroOrNeg:
        return 3;
    case Kind::One:
        return 4;
    case Kind::Positive:
        return 5;
    case Kind::ZeroOrPos:
        return 6;
    case Kind::Top:
        return 7;
    }
    return 7;
}

using TransferTable = Kind[8][8];

constexpr Kind B = Kind::Bottom;
constexpr Kind Z = Kind::Zero;
constexpr Kind N = Kind::Negative;
constexpr Kind ZN = Kind::ZeroOrNeg;
constexpr Kind O = Kind::One;
constexpr Kind P = Kind::Positive;
constexpr Kind ZP = Kind::ZeroOrPos;
constexpr Kind T = Kind::Top;

constexpr TransferTable addTable = {
    {B, B, B, B, B, B, B, B},  {B, Z, N, ZN, O, P, ZP, T}, {B, N, N, N, ZN, T, T, T},  {B, ZN, N, ZN, T, T, T, T},
    {B, O, ZN, T, P, P, P, T}, {B, P, T, T, P, P, P, T},   {B, ZP, T, T, P, P, ZP, T}, {B, T, T, T, T, T, T, T},
};

constexpr TransferTable subTable = {
    {B, B, B, B, B, B, B, B},  {B, Z, P, ZP, N, N, ZN, T}, {B, N, T, T, N, N, N, T},   {B, ZN, T, T, N, N, ZN, T},
    {B, O, P, P, Z, ZN, T, T}, {B, P, P, P, ZP, T, T, T},  {B, ZP, P, ZP, T, T, T, T}, {B, T, T, T, T, T, T, T},
};

constexpr TransferTable mulTable = {
    {B, B, B, B, B, B, B, B},   {B, Z, Z, Z, Z, Z, Z, Z},   {B, Z, P, ZP, N, N, ZN, T},    {B, Z, ZP, ZP, ZN, ZN, ZN, T},
    {B, Z, N, ZN, O, P, ZP, T}, {B, Z, N, ZN, P, P, ZP, T}, {B, Z, ZN, ZN, ZP, ZP, ZP, T}, {B, Z, T, T, T, T, T, T},
};

constexpr TransferTable divTable = {
    {B, B, B, B, B, B, B, B},     {B, B, Z, Z, Z, Z, Z, Z},     {B, B, ZP, ZP, N, ZN, ZN, T},  {B, B, ZP, ZP, ZN, ZN, ZN, T},
    {B, B, ZN, ZN, O, ZP, ZP, T}, {B, B, ZN, ZN, P, ZP, ZP, T}, {B, B, ZN, ZN, ZP, ZP, ZP, T}, {B, B, T, T, T, T, T, T},
};

constexpr TransferTable greaterThanTable = {
    {B, B, B, B, B, B, B, B},   {B, Z, O, ZP, Z, Z, Z, ZP},   {B, Z, ZP, ZP, Z, Z, Z, ZP},    {B, Z, ZP, ZP, Z, Z, Z, ZP},
    {B, O, O, O, Z, Z, ZP, ZP}, {B, O, O, O, ZP, ZP, ZP, ZP}, {B, ZP, O, ZP, ZP, ZP, ZP, ZP}, {B, ZP, ZP, ZP, ZP, ZP, ZP, ZP},
};

constexpr TransferTable equalTable = {
    {B, B, B, B, B, B, B, B},    {B, O, Z, ZP, Z, Z, ZP, ZP},  {B, Z, ZP, ZP, Z, Z, Z, ZP},    {B, ZP, ZP, ZP, Z, Z, ZP, ZP},
    {B, Z, Z, Z, O, ZP, ZP, ZP}, {B, Z, Z, Z, ZP, ZP, ZP, ZP}, {B, ZP, Z, ZP, ZP, ZP, ZP, ZP}, {B, ZP, ZP, ZP, ZP, ZP, ZP, ZP},
};

constexpr TransferTable andTable = {
    {B, B, B, B, B, B, B, B},      {B, Z, Z, Z, Z, Z, Z, Z},       {B, Z, N, ZN, ZP, ZP, ZP, T},   {B, Z, ZN, ZN, ZP, ZP, ZP, T},
    {B, Z, ZP, ZP, O, ZP, ZP, ZP}, {B, Z, ZP, ZP, ZP, ZP, ZP, ZP}, {B, Z, ZP, ZP, ZP, ZP, ZP, ZP}, {B, Z, T, T, ZP, ZP, ZP, T},
};

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

    // Constants seed the analysis with zero, one, negative, or positive.
    // This is the only rule that does not consult its operands, and without some
    // rule of this kind the analysis would have no facts to propagate at all.
    IntegerAttr value;
    if (matchPattern(op, m_Constant(&value))) {
        SignState state;
        if (value.getValue().isZero()) {
            state = Kind::Zero;
        } else if (value.getValue().isOne()) {
            state = Kind::One;
        } else if (value.getValue().isStrictlyPositive()) {
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
