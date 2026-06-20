# KKProjekat - Sparse Conditional Constant Propagation (SCCP) for LLVM

An implementation of the SCCP optimization pass for LLVM, built as a project for a Compiler Construction course.

## What this pass does

SCCP (Sparse Conditional Constant Propagation) is a classic compiler optimization pass (Wegman & Zadeck) that:

1. **Propagates constants** through SSA def-use chains - if all operands of an instruction are constants, it folds the result.
2. **Accounts for branch feasibility** (the "Conditional" part of the name) - if a branch's condition is a known constant, only that one successor is considered reachable, which avoids incorrectly propagating information through dead code.
3. **Cleans up the result** - replaces folded instructions with constants, deletes dead instructions, turns conditional branches with a constant condition into unconditional ones, and removes unreachable blocks.

The pass is registered as `kksccp` in the New Pass Manager and is invoked via:

```bash
opt -passes=kksccp -S input.ll
```

## Theoretical background

### Lattice

Every SSA value has one of three possible states, moving strictly monotonically `Top → Constant → Bottom` (a value never moves backwards):

- **Top (⊤)** - we don't know anything about the value yet
- **Constant** - we know it is exactly equal to a specific constant
- **Bottom (⊥)** - proven to not be a (single) constant

Function arguments are immediately initialized to `Bottom`, since intraprocedural analysis cannot know their value at the call site.

### Two worklists

- **`FlowWorklist`** - blocks that have just become executable, waiting for their instructions to be processed.
- **`SSAWorklist`** - values whose lattice state has changed, waiting for their users (`users()`) to be re-evaluated.

### `KnownFeasibleEdges`

A set of CFG edges (pairs of `(Source, Dest)` blocks) that are provably feasible. This is essential for correctly handling PHI nodes - a PHI must ignore operands coming from edges that have never (or not yet) been marked feasible, even if the predecessor block is otherwise reachable through some other path.

## Project structure

```
llvm/
├── include/llvm/Transforms/KKProjekat/
│   └── SCCP.h                    # MySCCPPass class declaration
└── lib/Transforms/KKProjekat/
    ├── SCCP.cpp                  # implementation (lattice, solver, rewrite)
    └── CMakeLists.txt            # add_llvm_component_library

llvm/test/Transforms/KKProjekat/
├── test1.ll                      # basic constant folding + branch/CFG fold
├── test2.ll                      # PHI with two different constants -> Bottom
└── test3.ll                      # loop with an induction variable -> Bottom

# Modified existing LLVM files (build plumbing):
llvm/lib/Transforms/CMakeLists.txt      # add_subdirectory(KKProjekat)
llvm/lib/Passes/CMakeLists.txt          # KKProjekat in LINK_COMPONENTS
llvm/lib/Passes/PassBuilder.cpp         # #include SCCP.h
llvm/lib/Passes/PassRegistry.def        # FUNCTION_PASS("kksccp", MySCCPPass())
llvm/tools/opt/CMakeLists.txt           # KKProjekat in LLVM_LINK_COMPONENTS
```

## Build

```bash
cd build
cmake -G Ninja ../llvm \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

ninja opt
```

## Testing

Manual:

```bash
./bin/opt -passes=kksccp -S test1.ll
```

Formal tests via `lit`/`FileCheck`:

```bash
./bin/llvm-lit ../llvm/test/Transforms/KKProjekat/ -v
```

## Algorithm - brief overview

```
markEntryExecutable()
while SSAWorklist or FlowWorklist is non-empty:
    while FlowWorklist is non-empty:
        pop block B, process every instruction in B (visitInstruction)
    while SSAWorklist is non-empty:
        pop value V, re-process every user of V

visitInstruction(I):
    if I->getParent() is not executable -> ignore
    binary op -> fold if both operands are Constant, else Bottom if either is Bottom
    cmp -> same, via ConstantFoldCompareInstruction
    branch -> if condition is Constant, mark only one successor edge feasible; if Bottom, mark both
    phi -> meet only over operands whose incoming edge is in KnownFeasibleEdges
```

After reaching a fixpoint:
1. Replace all `Constant` values in the IR (`replaceAllUsesWith`)
2. Delete dead instructions (`use_empty`)
3. Fold conditional branches with a constant condition into unconditional ones
4. `removeUnreachableBlocks` - remove unreachable blocks



## Authors

Sofija Višnjić 57/2020 <br>
Jana Radojević 184/2019
