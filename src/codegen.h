#pragma once

#include "ast.h"
#include "cimport.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>

// `tm` describes the machine we compile for (sizes, alignment), which shapes the IR.
std::unique_ptr<llvm::Module> generate(const Program &prog, const CImports &c, llvm::LLVMContext &ctx,
                                       llvm::TargetMachine &tm);
