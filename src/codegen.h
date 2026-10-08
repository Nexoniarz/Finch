#pragma once

#include "ast.h"
#include "cimport.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>

// All files of the program (the main file first, then its modules) become one LLVM module.
// `tm` describes the machine we compile for (sizes, alignment), which shapes the IR.
std::unique_ptr<llvm::Module> generate(const std::vector<Program> &progs, const CImports &c, llvm::LLVMContext &ctx,
                                       llvm::TargetMachine &tm, bool debug);
