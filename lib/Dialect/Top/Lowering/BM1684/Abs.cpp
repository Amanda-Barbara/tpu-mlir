//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "../Lowering.h"
#include "tpu_mlir/Dialect/Top/IR/TopOps.h"
#include "tpu_mlir/Support/Dnnl/Dnnl.h"
#include "tpu_mlir/Support/Helper/Module.h"

using namespace tpu_mlir;
using namespace tpu_mlir::helper;
using namespace mlir;

Value top::AbsOp::lowering_int8_bm1684() {
  llvm_unreachable("AbsOp to be supported");
  return nullptr;
}

Value top::AbsOp::lowering_f32_bm1684() {
  return lowering_common_float<tpu::AbsOp, Float32Type>(getOperation());
}