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

// Keeping the tables in the same form as the specification makes each
// transfer function easy to check against it.
using TransferTable = Kind[kKindCount][kKindCount];

SignState evaluate(const TransferTable& table, SignState lhs, SignState rhs) { return table[kindIndex(lhs.kind)][kindIndex(rhs.kind)]; }

constexpr Kind Bo = Kind::Bottom;
constexpr Kind Ze = Kind::Zero;
constexpr Kind Ne = Kind::Negative;
constexpr Kind ZN = Kind::ZeroOrNeg;
constexpr Kind On = Kind::One;
constexpr Kind Po = Kind::Positive;
constexpr Kind ZP = Kind::ZeroOrPos;
constexpr Kind To = Kind::Top;

constexpr TransferTable addTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, Ze, Ne, ZN, On, Po, ZP, To},
    /* Ne */ {Bo, Ne, Ne, Ne, ZN, To, To, To},
    /* ZN */ {Bo, ZN, Ne, ZN, To, To, To, To},
    /* On */ {Bo, On, ZN, To, Po, Po, Po, To},
    /* Po */ {Bo, Po, To, To, Po, Po, Po, To},
    /* ZP */ {Bo, ZP, To, To, Po, Po, ZP, To},
    /* To */ {Bo, To, To, To, To, To, To, To},
};

constexpr TransferTable subTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, Ze, Po, ZP, Ne, Ne, ZN, To},
    /* Ne */ {Bo, Ne, To, To, Ne, Ne, Ne, To},
    /* ZN */ {Bo, ZN, To, To, Ne, Ne, ZN, To},
    /* On */ {Bo, On, Po, Po, Ze, ZN, To, To},
    /* Po */ {Bo, Po, Po, Po, ZP, To, To, To},
    /* ZP */ {Bo, ZP, Po, ZP, To, To, To, To},
    /* To */ {Bo, To, To, To, To, To, To, To},
};

constexpr TransferTable multTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, Ze, Ze, Ze, Ze, Ze, Ze, Ze},
    /* Ne */ {Bo, Ze, Po, ZP, Ne, Ne, ZN, To},
    /* ZN */ {Bo, Ze, ZP, ZP, ZN, ZN, ZN, To},
    /* On */ {Bo, Ze, Ne, ZN, On, Po, ZP, To},
    /* Po */ {Bo, Ze, Ne, ZN, Po, Po, ZP, To},
    /* ZP */ {Bo, Ze, ZN, ZN, ZP, ZP, ZP, To},
    /* To */ {Bo, Ze, To, To, To, To, To, To},
};

constexpr TransferTable divTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, Bo, Ze, Ze, Ze, Ze, Ze, Ze},
    /* Ne */ {Bo, Bo, ZP, ZP, Ne, ZN, ZN, To},
    /* ZN */ {Bo, Bo, ZP, ZP, ZN, ZN, ZN, To},
    /* On */ {Bo, Bo, ZN, ZN, On, ZP, ZP, To},
    /* Po */ {Bo, Bo, ZN, ZN, Po, ZP, ZP, To},
    /* ZP */ {Bo, Bo, ZN, ZN, ZP, ZP, ZP, To},
    /* To */ {Bo, Bo, To, To, To, To, To, To},
};

constexpr TransferTable greaterThanTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, Ze, On, ZP, Ze, Ze, Ze, ZP},
    /* Ne */ {Bo, Ze, ZP, ZP, Ze, Ze, Ze, ZP},
    /* ZN */ {Bo, Ze, ZP, ZP, Ze, Ze, Ze, ZP},
    /* On */ {Bo, On, On, On, Ze, Ze, ZP, ZP},
    /* Po */ {Bo, On, On, On, ZP, ZP, ZP, ZP},
    /* ZP */ {Bo, ZP, On, ZP, ZP, ZP, ZP, ZP},
    /* To */ {Bo, ZP, ZP, ZP, ZP, ZP, ZP, ZP},
};

constexpr TransferTable equalTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, On, Ze, ZP, Ze, Ze, ZP, ZP},
    /* Ne */ {Bo, Ze, ZP, ZP, Ze, Ze, Ze, ZP},
    /* ZN */ {Bo, ZP, ZP, ZP, Ze, Ze, ZP, ZP},
    /* On */ {Bo, Ze, Ze, Ze, On, ZP, ZP, ZP},
    /* Po */ {Bo, Ze, Ze, Ze, ZP, ZP, ZP, ZP},
    /* ZP */ {Bo, ZP, Ze, ZP, ZP, ZP, ZP, ZP},
    /* To */ {Bo, ZP, ZP, ZP, ZP, ZP, ZP, ZP},
};

constexpr TransferTable andTable = {
    //        Bo. Ze. Ne. ZN. On. Po. ZP. To
    /* Bo */ {Bo, Bo, Bo, Bo, Bo, Bo, Bo, Bo},
    /* Ze */ {Bo, Ze, Ze, Ze, Ze, Ze, Ze, Ze},
    /* Ne */ {Bo, Ze, Ne, ZN, ZP, ZP, ZP, To},
    /* ZN */ {Bo, Ze, ZN, ZN, ZP, ZP, ZP, To},
    /* On */ {Bo, Ze, ZP, ZP, On, ZP, ZP, ZP},
    /* Po */ {Bo, Ze, ZP, ZP, ZP, ZP, ZP, ZP},
    /* ZP */ {Bo, Ze, ZP, ZP, ZP, ZP, ZP, ZP},
    /* To */ {Bo, Ze, To, To, ZP, ZP, ZP, To},
};

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
    const TransferTable* table = nullptr;

    if (isa<LLVM::AddOp>(op))
        table = &addTable;
    if (isa<LLVM::SubOp>(op))
        table = &subTable;
    if (isa<LLVM::MulOp>(op))
        table = &multTable;
    if (isa<LLVM::SDivOp>(op))
        table = &divTable;
    if (isa<LLVM::AndOp>(op))
        table = &andTable;

    if (auto compare = dyn_cast<LLVM::ICmpOp>(op)) {
        if (compare.getPredicate() == LLVM::ICmpPredicate::sgt)
            table = &greaterThanTable;
        else if (compare.getPredicate() == LLVM::ICmpPredicate::eq)
            table = &equalTable;
    }

    if (table != nullptr) {
        propagateIfChanged(result, result->join(evaluate(*table, lhs, rhs)));
        return success();
    }

    return unknown();
}

} // namespace sign
