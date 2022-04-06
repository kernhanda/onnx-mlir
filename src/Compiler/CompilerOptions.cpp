/*
 * SPDX-License-Identifier: Apache-2.0
 */

//===------------------------ CompilerOptions.cpp -------------------------===//
//
// Copyright 2022 The IBM Research Authors.
//
// =============================================================================
//
// Functions for adding options.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/Debug.h"

#include "ExternalUtil.hpp"
#include "onnx-mlir/Compiler/OMCompilerTypes.h"
#include "src/Compiler/CompilerOptions.hpp"

#define DEBUG_TYPE "compiler_options"

namespace onnx_mlir {
llvm::cl::OptionCategory OnnxMlirOptions(
    "ONNX-MLIR Options", "These are frontend options.");

// the option is used in this file, so defined here
llvm::cl::opt<bool> invokeOnnxVersionConverter("invokeOnnxVersionConverter",
    llvm::cl::desc(
        "call onnx version converter to convert ONNX model to current version"),
    llvm::cl::init(false), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<bool> preserveLocations("preserveLocations",
    llvm::cl::desc("emit location data:"), llvm::cl::init(false),
    llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<bool> printIR("printIR",
    llvm::cl::desc("print the IR to stdout:"), llvm::cl::init(false),
    llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<bool> preserveBitcode("preserveBitcode",
    llvm::cl::desc(
        "dont delete the bitcode files (optimized and unoptimized):"),
    llvm::cl::init(false), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<bool> preserveMLIR("preserveMLIR",
    llvm::cl::desc("dont delete the MLIR files (input and llvm):"),
    llvm::cl::init(false), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<bool> useOnnxModelTypes("useOnnxModelTypes",
    llvm::cl::desc("use types and shapes from ONNX model"),
    llvm::cl::init(false), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<int> repeatOnnxTransform("repeatOnnxTransform",
    llvm::cl::desc(
        "invoke extra onnx transform pass(shape inference, constant and etc.)"),
    llvm::cl::init(0), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<std::string> shapeInformation("shapeInformation",
    llvm::cl::desc(
        "Custom shapes for the inputs of the ONNX model, e.g. setting static "
        "shapes for dynamic inputs.\n"
        "\"value\" is in the format of "
        "\"INPUT_ID1:D1xD2x...xDn,INPUT_ID2:D1xD2x...xDn, ...\",\n"
        "where \"INPUT_ID1, INPUT_ID2, ...\" are input indices starting from "
        "0, and\n"
        "\"D1, D2, ...\" are dimension sizes (positive integers of -1 for "
        "unknown dimensions)"),
    llvm::cl::value_desc("value"), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<std::string> mtriple("mtriple",
    llvm::cl::desc("Override target triple for module"),
    llvm::cl::value_desc("LLVM target triple"), llvm::cl::cat(OnnxMlirOptions),
    llvm::cl::ValueRequired);

llvm::cl::opt<std::string> mcpu("mcpu", llvm::cl::desc("Target cpu"),
    llvm::cl::value_desc("Target a specific CPU type"),
    llvm::cl::cat(OnnxMlirOptions), llvm::cl::ValueRequired);

llvm::cl::opt<std::string> march("march",
    llvm::cl::desc("Target architecture to generate code for"),
    llvm::cl::value_desc("Target a specific architecture type"),
    llvm::cl::cat(OnnxMlirOptions), llvm::cl::ValueRequired);

llvm::cl::list<accel::Accelerator::Kind> maccel("maccel",
    llvm::cl::desc("Specify an accelerator to generate code for"),
    // clang-format off
    llvm::cl::values(
      APPLY_TO_ACCELERATORS(CREATE_ACCEL_CL_ENUM)
      clEnumValN(accel::Accelerator::Kind::NONE, "NONE", "No accelerator")
    ),
    // clang-format on
    llvm::cl::cat(OnnxMlirOptions), llvm::cl::ValueRequired);

llvm::cl::opt<bool> VerboseOutput("v", llvm::cl::desc("Use verbose output"),
    llvm::cl::init(false), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::opt<std::string> Xopt("Xopt",
    llvm::cl::desc("Arguments to forward to LLVM's 'opt' option processing"),
    llvm::cl::value_desc("A valid LLVM's 'opt' option"),
    llvm::cl::cat(OnnxMlirOptions), llvm::cl::Hidden, llvm::cl::ValueRequired);

llvm::cl::opt<std::string> Xllc("Xllc",
    llvm::cl::desc("Arguments to forward to LLVM's 'llc' option processing"),
    llvm::cl::value_desc("A valid LLVM's 'llc' option"),
    llvm::cl::cat(OnnxMlirOptions), llvm::cl::Hidden, llvm::cl::ValueRequired);

llvm::cl::opt<std::string> mllvm("mllvm",
    llvm::cl::desc(
        "Arguments to forward to LLVM's 'opt' and 'llc' option processing"),
    llvm::cl::value_desc("A valid LLVM's 'opt' and 'llc' option"),
    llvm::cl::cat(OnnxMlirOptions), llvm::cl::Hidden, llvm::cl::ValueRequired);

llvm::cl::opt<OptLevel> OptimizationLevel(
    llvm::cl::desc("Optimization levels:"),
    llvm::cl::values(clEnumVal(O0, "Optimization level 0 (default)."),
        clEnumVal(O1, "Optimization level 1."),
        clEnumVal(O2, "Optimization level 2."),
        clEnumVal(O3, "Optimization level 3.")),
    llvm::cl::init(O0), llvm::cl::cat(OnnxMlirOptions));

llvm::cl::OptionCategory OMPassOptions("ONNX-MLIR Pass Options",
    "These are options to provide fine control on passes");

llvm::cl::opt<std::string> instrumentONNXOps("instrument-onnx-ops",
    llvm::cl::desc("Specify onnx ops to be instrumented\n"
                   "\"NONE\" or \"\" for no instrument\n"
                   "\"ALL\" for all ops. \n"
                   "\"op1 op2 ...\" for the specified ops."),
    llvm::cl::init(""), llvm::cl::cat(OMPassOptions));

llvm::cl::opt<bool> enableMemoryBundling("enable-memory-bundling",
    llvm::cl::desc(
        "Enable memory bundling related optimizations (default=false)\n"
        "Set to 'false' if you experience significant compile time."),
    llvm::cl::init(false), llvm::cl::cat(OMPassOptions));

llvm::cl::opt<int> onnxOpTransformThreshold("onnx-op-transform-threshold",
    llvm::cl::desc(
        "Max iteration for dynamic op transform passes (default=3).\n"
        "If set to 0, onnxOpTransformPass will be disabled, and\n"
        "static iteration will be used"),
    llvm::cl::init(3), llvm::cl::cat(OMPassOptions));

llvm::cl::opt<bool> onnxOpTransformReport("onnx-op-transform-report",
    llvm::cl::desc("Report diagnostic info for op transform passes."),
    llvm::cl::init(false), llvm::cl::cat(OMPassOptions));

// =============================================================================
// Methods for setting and getting compiler variables.

// Triple.
void setTargetTriple(const std::string &triple) {
  LLVM_DEBUG(llvm::dbgs() << DEBUG_TYPE << "Set triple\"" << triple << "\"\n");
  mtriple = triple;
}

std::string getTargetTripleOption() {
  std::string targetOptions = "";
  // Command cannot tolerate extra spaces. Add only when needed.
  if (mtriple != "")
    targetOptions = "--mtriple=" + mtriple;
  else if (kDefaultTriple != "")
    targetOptions = "--mtriple=" + kDefaultTriple;
  return targetOptions;
}

// Arch.
void setTargetArch(const std::string &arch) {
  LLVM_DEBUG(llvm::dbgs() << DEBUG_TYPE << "Set arch\"" << arch << "\"\n");
  march = arch;
}

std::string getTargetArchOption() {
  return (march != "") ? "--march=" + march : "";
}

// CPU.
void setTargetCPU(const std::string &cpu) {
  LLVM_DEBUG(llvm::dbgs() << DEBUG_TYPE << "Set CPU\"" << cpu << "\"\n");
  mcpu = cpu;
}

std::string getTargetCPUOption() {
  return (mcpu != "") ? "--mcpu=" + mcpu : "";
}

// Optimization level.
void setOptLevel(const OptLevel level) {
  LLVM_DEBUG(llvm::dbgs() << DEBUG_TYPE << "Set opt level " << level << "\n");
  OptimizationLevel = level;
}

std::string getOptimizationLevelOption() {
  switch (OptimizationLevel) {
  case OptLevel::O0:
    return "-O0";
  case OptLevel::O1:
    return "-O1";
  case OptLevel::O2:
    return "-O2";
  case OptLevel::O3:
    return "-O3";
  }
  llvm_unreachable("Unexpected optimization level");
  return "";
}

// Xopt.
void setXoptOption(const std::string &flag) { Xopt = flag; }

std::string getXoptOption() { return (Xopt != "") ? Xopt : std::string(); }

// Xllc.
void setXllcOption(const std::string &flag) { Xllc = flag; }

std::string getXllcOption() { return (Xllc != "") ? Xllc : std::string(); }

// LLVM.
void setLLVMOption(const std::string &flag) { mllvm = flag; }

std::string getLLVMOption() { return (mllvm != "") ? mllvm : std::string(); }

// =============================================================================
// Methods for OMCompilerOptions

int setCompilerOption(const OptionKind kind, const std::string &val) {
  switch (kind) {
  case OptionKind::TargetTriple:
    setTargetTriple(val);
    break;
  case OptionKind::TargetArch:
    setTargetArch(val);
    break;
  case OptionKind::TargetCPU:
    setTargetCPU(val);
    break;
  case OptionKind::CompilerOptLevel: {
    int level = atoi(val.c_str());
    if (level < 0 || level > 3)
      return 1;
    setOptLevel((OptLevel)level);
  } break;
  case OptionKind::OPTFlag:
    setXoptOption(val);
    break;
  case OptionKind::LLCFlag:
    setXllcOption(val);
    break;
  case OptionKind::LLVMFlag:
    setLLVMOption(val);
    break;
    // Ignore options that were added but are unknown.
  }
  return 0;
}

std::string getCompilerOption(const OptionKind kind) {
  switch (kind) {
  case OptionKind::TargetTriple:
    return getTargetTripleOption();
  case OptionKind::TargetArch:
    return getTargetArchOption();
  case OptionKind::TargetCPU:
    return getTargetCPUOption();
  case OptionKind::CompilerOptLevel:
    return getOptimizationLevelOption();
  case OptionKind::OPTFlag:
    return getXoptOption();
  case OptionKind::LLCFlag:
    return getXllcOption();
  case OptionKind::LLVMFlag:
    return getLLVMOption();
  }
  return std::string();
}

int setCompilerOptions(const CompilerOptionList &list) {
  for (const auto &pair : list) {
    int rc = setCompilerOption(pair.first, pair.second);
    if (rc != 0)
      return rc;
  }
  return 0;
}

} // namespace onnx_mlir
