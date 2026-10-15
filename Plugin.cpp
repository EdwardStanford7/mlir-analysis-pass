//===- Plugin.cpp - Pass definition and plugin entry point ----------------===//
//
// Scaffolding: wires the analysis into a pass and exposes it to mlir-opt.
//
//===----------------------------------------------------------------------===//

#include "Annotate.h"
#include "Instrumentation.h"
#include "SignAnalysis.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Analysis/DataFlow/ConstantPropagationAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

#include <utility>

using namespace mlir;

namespace {

struct SignAnalysisPass : PassWrapper<SignAnalysisPass, OperationPass<ModuleOp>> {
    MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(SignAnalysisPass)

    SignAnalysisPass() = default;
    SignAnalysisPass(const SignAnalysisPass& other) : PassWrapper(other) {}

    Option<bool> instrument{*this, "instrument", llvm::cl::desc("Insert runtime checks for inferred integer signs"), llvm::cl::init(false)};

    StringRef getArgument() const final { return "sign-analysis"; }

    StringRef getDescription() const final { return "Determine the sign of integer values"; }

    void runOnOperation() override {
        DataFlowConfig config;
        config.setInterprocedural(false);

        DataFlowSolver solver(config);
        // DeadCodeAnalysis supplies reachability, without which the solver must
        // assume every branch is taken; SparseConstantPropagation resolves branch
        // conditions for it.  Both are prerequisites, not extras.
        solver.load<dataflow::DeadCodeAnalysis>();
        solver.load<dataflow::SparseConstantPropagation>();
        solver.load<sign::SignAnalysis>();

        if (failed(solver.initializeAndRun(getOperation()))) {
            getOperation().emitError("sign analysis failed to reach a fixed point");
            return signalPassFailure();
        }

        // Save the original facts before instrumentation changes the control flow.
        llvm::SmallVector<std::pair<Value, sign::Kind>> checks;
        auto collect = [&](Value value) {
            auto type = llvm::dyn_cast<IntegerType>(value.getType());
            if (!type || type.getWidth() == 1)
                return;
            const auto* lattice = solver.lookupState<sign::SignLattice>(value);
            if (!lattice)
                return;
            sign::Kind kind = lattice->getValue().kind;
            if (kind != sign::Kind::Top && kind != sign::Kind::Bottom)
                checks.emplace_back(value, kind);
        };
        if (instrument) {
            getOperation().walk([&](LLVM::LLVMFuncOp function) {
                for (Block& block : function.getBody()) {
                    for (BlockArgument argument : block.getArguments())
                        collect(argument);
                    for (Operation& op : block)
                        for (Value result : op.getResults())
                            collect(result);
                }
            });
        }

        // Query states only now that the solver has converged.
        auto describe = [&](Value value, AsmState& asmState) -> std::string {
            const auto* lattice = solver.lookupState<sign::SignLattice>(value);
            if (!lattice)
                return {};
            sign::Kind kind = lattice->getValue().kind;
            // Top and bottom say nothing; printing them would bury the real facts.
            if (kind == sign::Kind::Top || kind == sign::Kind::Bottom)
                return {};
            std::string description;
            llvm::raw_string_ostream os(description);
            value.printAsOperand(os, asmState);
            os << " is " << sign::name(kind);

            return description;
        };

        // Print original annotations to stderr; mlir-opt prints the resulting
        // module to stdout after this pass returns.
        sign::printAnnotated(getOperation(), describe, llvm::errs());

        for (const auto& [value, kind] : checks)
            sign::insert_assertion(kind, value);

        if (!instrument)
            markAllAnalysesPreserved();
    }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
    // LLVM_VERSION_STRING is baked in at compile time and checked by mlir-opt at
    // load time, which is what turns an ABI mismatch into a clear diagnostic.
    return {MLIR_PLUGIN_API_VERSION, "SignAnalysis", LLVM_VERSION_STRING, []() { PassRegistration<SignAnalysisPass>(); }};
}
