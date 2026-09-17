//===- SignAnalysis.cpp - Transfer functions ------------------------------===//
//
// The transfer function: given what is known about an operation's operands,
// state what is known about its results.
//
//===----------------------------------------------------------------------===//

#include "SignAnalysis.h"

#include "mlir/IR/Matchers.h"

using namespace mlir;

namespace sign {

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
    IntegerAttr value;
    if (matchPattern(op, m_Constant(&value))) {
        SignState state;
        if (value.getValue().isZero()) {
            state = Kind::Zero;
        } else if (value.getValue().isStrictlyPositive()) {
            state = Kind::Positive;
        } else if (value.getValue().isNegative()) {
            state = Kind::Negative;
        }

        propagateIfChanged(result, result->join(state));
        return success();
    }

    return unknown();
}

} // namespace sign
