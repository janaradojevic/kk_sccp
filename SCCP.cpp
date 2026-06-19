#include "SCCP.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ConstantFold.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace {


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

  void markEntryExecutable(){
    BasicBlock *Entry = &F.getEntryBlock();
    ExecutableBlocks.insert(Entry);
    FlowWorklist.push_back(Entry);

  }

  LatticeVal getLatticeVal(Value *V) {
    if (auto *C = dyn_cast<Constant>(V)) {
        LatticeVal LV;
        LV.State = LatticeState::Constant;
        LV.Val = C;
        return LV;
    }
    return Lattice[V];
  }
  
void markEdgeExecutable(BasicBlock *Source, BasicBlock *Dest) {
    std::pair<BasicBlock*, BasicBlock*> Edge(Source, Dest);
    
    if (KnownFeasibleEdges.find(Edge) != KnownFeasibleEdges.end())
        return;
    KnownFeasibleEdges.insert(Edge);
    
    if (ExecutableBlocks.find(Dest) == ExecutableBlocks.end()) {
        ExecutableBlocks.insert(Dest);
        FlowWorklist.push_back(Dest);
    } 
    
    else {
        for (PHINode &PN : Dest->phis()) {
            SSAWorklist.push_back(&PN);
        }
    }
}


  void visitBlock(BasicBlock *BB){
    for (Instruction &I : *BB) {
        visitInstruction(&I);
    }

  }

  void visitInstruction(Instruction *I){
    
    if (ExecutableBlocks.find(I->getParent()) == ExecutableBlocks.end())
        return;

    
    if (I->isBinaryOp()) {
        visitBinaryOp(I);
    } else if (auto *Cmp = dyn_cast<CmpInst>(I)) {
        visitCmp(Cmp);
    } else if (auto *BI = dyn_cast<BranchInst>(I)) {
        visitBranch(BI);
    } else if (auto *PHI = dyn_cast<PHINode>(I)) {
        visitPHI(PHI);
    } else if (I->getType()->isVoidTy()) {
        //is void
    } else {
        markOverdefined(I);
    }

  }

 
  void visitPHI(PHINode *PHI){
  
    bool SawConstant = false;
    bool SawBottom = false;
    Constant *PhiConst = nullptr;

    for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
      BasicBlock *PredBB = PHI->getIncomingBlock(i);
      Value *InVal = PHI->getIncomingValue(i);

      std::pair<BasicBlock *, BasicBlock *> Edge(PredBB, PHI->getParent());
      if (KnownFeasibleEdges.find(Edge) == KnownFeasibleEdges.end())
        continue;

      LatticeVal InLV = getLatticeVal(InVal);

      if (InLV.State == LatticeState::Top) {
        continue;
      }

      if (InLV.State == LatticeState::Bottom) {
        SawBottom = true;
        break;
      }

      if (!SawConstant) {
        SawConstant = true;
        PhiConst = InLV.Val;
      } else if (PhiConst != InLV.Val) {
        SawBottom = true;
        break;
      }
    }

    if (SawBottom) {
      markOverdefined(PHI);
    } else if (SawConstant) {
      markConstant(PHI, PhiConst);
    }
  }


  void visitBinaryOp(Instruction *I){
    
  }


  void visitCmp(Instruction *I){
    auto *Cmp = dyn_cast<CmpInst>(I);
    if (!Cmp) return;

    Value *LeftOp = Cmp->getOperand(0);
    Value *RightOp = Cmp->getOperand(1);

    LatticeVal LeftLV = getLatticeVal(LeftOp);
    LatticeVal RightLV = getLatticeVal(RightOp);

    if (LeftLV.State == LatticeState::Bottom || RightLV.State == LatticeState::Bottom) {
        markOverdefined(Cmp);
        return;
    }

    if (LeftLV.State == LatticeState::Constant && RightLV.State == LatticeState::Constant) {
        
        Constant *ResultConstant = ConstantFoldCompareInstruction(Cmp->getPredicate(), LeftLV.Val, RightLV.Val);

        if (ResultConstant) {
            markConstant(Cmp, ResultConstant);
            return;
        }
    }


  }
  void visitBranch(BranchInst *BI){
    if (BI->isConditional()) {
        Value *Condition = BI->getCondition();
        LatticeVal CondLV = getLatticeVal(Condition);

        if (CondLV.State == LatticeState::Top) {
            return;
        }

        if (CondLV.State == LatticeState::Constant) {
            auto *CI = dyn_cast<ConstantInt>(CondLV.Val);
            if (CI) {

                BasicBlock *TargetBlock = CI->isOne() ? BI->getSuccessor(0) : BI->getSuccessor(1);

                markEdgeExecutable(BI->getParent(), TargetBlock);
                return;
            }
        }

      // ako je stanje Bottom, oznacavamo oba puta kao izvrsiva
        markOverdefined(BI); 
        markEdgeExecutable(BI->getParent(), BI->getSuccessor(0));
        markEdgeExecutable(BI->getParent(), BI->getSuccessor(1));
        return;
    } 

    // bezuslovno grananje, označavamo samo jedan put
    else {
        markEdgeExecutable(BI->getParent(), BI->getSuccessor(0));
    }
}

  
  void markConstant(Value *V, Constant *C){
    LatticeVal &LV = Lattice[V];

    if (LV.State == LatticeState::Top) {
        LV.State = LatticeState::Constant;
        LV.Val = C;

        SSAWorklist.push_back(V);
    
      }
  }

  void markOverdefined(Value *V){
    LatticeVal &LV = Lattice[V];

    if (LV.State != LatticeState::Bottom) {
        LV.State = LatticeState::Bottom;
        LV.Val = nullptr; 

        SSAWorklist.push_back(V);
    }
}
  

  std::map<Value *, LatticeVal> Lattice;
  std::set<std::pair<BasicBlock*, BasicBlock*>> KnownFeasibleEdges;
  std::set<BasicBlock *> ExecutableBlocks;
  std::vector<Value *> SSAWorklist;
  std::vector<BasicBlock *> FlowWorklist;
  Function &F;
};

} 

PreservedAnalyses MySCCPPass::run(Function &F, FunctionAnalysisManager &AM) {
  KKSolver Solver(F);
  Solver.run();

  // TODO
  return PreservedAnalyses::none();
}