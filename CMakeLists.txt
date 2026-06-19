add_llvm_component_library(LLVMKKProjekat
  SCCP.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Core
  Support
  )