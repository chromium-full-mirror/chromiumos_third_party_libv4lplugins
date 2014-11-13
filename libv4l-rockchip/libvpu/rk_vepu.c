/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "libvpu/rk_vepu_interface.h"

#include <stdlib.h>

#define NUM_CTRLS 3

static uint32_t rk_ctrl_ids[NUM_CTRLS] = {
  V4L2_CID_PRIVATE_RK3288_HEADER,
  V4L2_CID_PRIVATE_RK3288_REG_PARAMS,
  V4L2_CID_PRIVATE_RK3288_HW_PARAMS
};
static uint32_t rk_payloads[NUM_CTRLS] = {0};
static uint32_t rk_payload_sizes[NUM_CTRLS] = {
  sizeof(rk_payloads[0]), sizeof(rk_payloads[1]), sizeof(rk_payloads[2])
};

void *rk_vepu_init(struct rk_vepu_param *param) {
  return malloc(sizeof(int));
}

void rk_vepu_deinit(void *enc) {
  free(enc);
}

int rk_vepu_get_config(void *enc, size_t *num_ctrls, uint32_t **ctrl_ids,
                       void ***payloads, uint32_t **payload_sizes) {
  *num_ctrls = NUM_CTRLS;
  *ctrl_ids = rk_ctrl_ids;
  *payloads = (void **)rk_payloads;
  *payload_sizes = rk_payload_sizes;
  return 0;
}

int rk_vepu_update_config(void *enc, void *config, uint32_t config_size,
                          uint32_t buffer_size) {
  return 0;
}
