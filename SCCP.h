#ifndef LLVM_TRANSFORMS_KK_PROJECT_MYSCCP_H
#define LLVM_TRANSFORMS_KK_PROJECT_MYSCCP_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class MySCCPPass : public PassInfoMixin<MySCCPPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif