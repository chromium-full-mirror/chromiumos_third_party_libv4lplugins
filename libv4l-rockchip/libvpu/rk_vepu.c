/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "libvpu/rk_vepu_interface.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libvpu/rk_vepu_debug.h"
#include "vp8_enc/rk_vp8encapi.h"

void *rk_vepu_init(struct rk_vepu_init_param *param) {
  int retval;
  struct rk_vp8_encoder *enc = rk_vp8_encoder_alloc_ctx();

  assert(param != NULL);
  if (enc == NULL) {
    VPU_PLG_ERR("Allocate encoder instance failed\n");
    return NULL;
  }

  enc->rk_ctrl_ids[0] = V4L2_CID_PRIVATE_RK3288_HEADER;
  enc->rk_ctrl_ids[1] = V4L2_CID_PRIVATE_RK3288_REG_PARAMS;
  enc->rk_ctrl_ids[2] = V4L2_CID_PRIVATE_RK3288_HW_PARAMS;

  retval = enc->ops->init(enc, param);
  if (retval < 0) {
    VPU_PLG_ERR("Encoder initialize failed\n");
    rk_vp8_encoder_free_ctx(enc);
    return NULL;
  }
  return enc;
}

void rk_vepu_deinit(void *enc) {
  struct rk_vp8_encoder *ienc = (struct rk_vp8_encoder*)enc;

  assert(enc != NULL);
  ienc->ops->deinit(ienc);
  rk_vp8_encoder_free_ctx(ienc);
}

int rk_vepu_get_config(void *enc, size_t *num_ctrls, uint32_t **ctrl_ids,
                       void ***payloads, uint32_t **payload_sizes)
{
  int retval;
  struct rk_vp8_encoder *ienc = (struct rk_vp8_encoder*)enc;

  assert(enc != NULL && num_ctrls != NULL && ctrl_ids != NULL);
  assert(payloads != NULL && payload_sizes != NULL);

  retval = ienc->ops->before_encode(ienc);
  if (retval < 0) {
    VPU_PLG_ERR("Generate configuration failed\n");
    return -1;
  }

  *num_ctrls = NUM_CTRLS;
  *ctrl_ids = ienc->rk_ctrl_ids;
  *payloads = (void **)ienc->rk_payloads;
  *payload_sizes = ienc->rk_payload_sizes;
  return 0;
}

int rk_vepu_update_config(void *enc, void *config, uint32_t config_size,
                          uint32_t buffer_size) {
  int retval;
  struct rk_vp8_encoder *ienc = (struct rk_vp8_encoder*)enc;

  assert(enc != NULL && config != NULL);

  retval = ienc->ops->updatepriv(ienc, config, config_size);
  if (retval < 0) {
    VPU_PLG_ERR("Update vp8 encoder private data failed\n");
    return -1;
  }
  return ienc->ops->after_encode(ienc, buffer_size);
}

int rk_vepu_update_parameter(void *enc,
                             struct rk_vepu_runtime_param *runtime_param) {
  struct rk_vp8_encoder *ienc = (struct rk_vp8_encoder*)enc;

  assert(enc != NULL && runtime_param != NULL);
  ienc->ops->updateparameter(ienc, runtime_param);
  return 0;
}
