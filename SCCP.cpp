#include "SCCP.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace {

// Lattice vrijednost za jedan SSA Value*
enum class LatticeState { Top, Constant, Bottom };

struct LatticeVal {
  LatticeState State = LatticeState::Top;
  Constant *Val = nullptr;
};

class KKSolver {
public:
  KKSolver(Function &F) : F(F) {}

  void run() {
    markEntryExecutable();
    while (!SSAWorklist.empty() || !FlowWorklist.empty()) {
      while (!FlowWorklist.empty()) {
        BasicBlock *BB = FlowWorklist.back();
        FlowWorklist.pop_back();
        visitBlock(BB);
      }
      while (!SSAWorklist.empty()) {
        Value *V = SSAWorklist.back();
        SSAWorklist.pop_back();
        for (User *U : V->users())
          if (auto *I = dyn_cast<Instruction>(U))
            visitInstruction(I);
      }
    }
  }

  // TODO: implementirati
  void markEntryExecutable();
  void visitBlock(BasicBlock *BB);
  void visitInstruction(Instruction *I);
  void visitPHI(PHINode *PHI);
  void visitBinaryOp(Instruction *I);
  void visitCmp(Instruction *I);
  void visitBranch(BranchInst *BI);

  // Pomoćno
  LatticeVal getLatticeVal(Value *V);
  void markConstant(Value *V, Constant *C);
  void markOverdefined(Value *V);

  std::map<Value *, LatticeVal> Lattice;
  std::set<BasicBlock *> ExecutableBlocks;
  std::vector<Value *> SSAWorklist;
  std::vector<BasicBlock *> FlowWorklist;
  Function &F;
};

} // namespace

PreservedAnalyses MySCCPPass::run(Function &F, FunctionAnalysisManager &AM) {
  KKSolver Solver(F);
  Solver.run();

  // TODO: prolaz koji zamjenjuje konstantne instrukcije i čisti dead grane

  return PreservedAnalyses::none();
}