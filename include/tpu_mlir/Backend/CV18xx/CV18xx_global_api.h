//===----------------------------------------------------------------------===//
//
// Copyright (c) 2020-2030 by Sophgo Technologies Inc. All rights reserved.
//
// Licensed under the Apache License v2.0.
// See http://www.apache.org/licenses/LICENSE-2.0 for license information.
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

#ifndef CVI_BACKEND_GLOBAL_API
#define CVI_BACKEND_GLOBAL_API

#include "tpu_mlir/Backend/CV18xx/CV18xx.h"

namespace tpu_mlir {
namespace backend {
void cvi_backend_tg_fixed_conv_kernel(
    const CviBackendContext &ctx, uint32_t layer_id, gaddr_t ga_ifmap,
    gaddr_t ga_ofmap, gaddr_t ga_weight, gaddr_t ga_bias, int input_n,
    int input_c, int input_h, int input_w, int groups, int output_c,
    uint16_t kh, uint16_t kw, uint16_t dilation_h, uint16_t dilation_w,
    uint8_t pad_top, uint8_t pad_bottom, uint8_t pad_left, uint8_t pad_right,
    uint8_t insert_h, uint8_t insert_w, uint8_t stride_h, uint8_t stride_w,
    int do_bias, int do_activation, float activation_arg[],
    int activation_gt_scale, int activation_gt_rshift, int activation_le_scale,
    int activation_le_rshift, int right_shift_width, bool do_chl_quan,
    bool do_ic_alignment, std::vector<uint8_t> *filter = nullptr,
    std::vector<uint8_t> *new_filter = nullptr, int pad_value = 0,
    gaddr_t ga_scale_lut = GA_INVALID);

void cvi_backend_tg_fixed_eltwise_add_kernel(
    const CviBackendContext &ctx, uint32_t layer_id,
    gaddr_t ga_inputs[], gaddr_t ga_output,
    int32_t operand_num, int32_t n, int32_t c,
    int32_t h, int32_t w, bool do_relu, bool do_early_stride,
    int32_t stride_h, int32_t stride_w, int32_t rshift,
    const int32_t *multipliers,
    const int32_t *coeffs);

void cvi_backend_tg_quant_kernel(
    const CviBackendContext &ctx,
    uint32_t layer_id,
    cvk_fmt_t from, cvk_fmt_t to,
    gaddr_t bottom_gaddr, gaddr_t top_gaddr,
    int input_n, int input_c, int input_h, int input_w,
    float const_scale = 1.0, int offset=0);

void cvi_backend_tg_fixed_avg_pooling_kernel(
    const CviBackendContext &ctx, uint32_t layer_id,
    gaddr_t ga_input, gaddr_t ga_output,
    int n, int c, int h, int w, int kh, int kw, int pad_top, int pad_bot,
    int pad_left, int pad_right, int stride_h, int stride_w,
    bool do_relu, int rshift, int multiplier, bool ceil_mode);
} // namespace backend
} // namespace tpu_mlir
#endif /* CVI_BACKEND_GLOBAL_API */
