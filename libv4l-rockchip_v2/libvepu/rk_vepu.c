/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "libvepu/rk_vepu_interface.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libvepu/rk_vepu_debug.h"
#include "h264e/h264e.h"
#include "vp8e/vp8e.h"

void *rk_vepu_init(struct rk_vepu_init_param *param) {
	int retval;
	struct rk_venc *enc;

	assert(param != NULL);

	switch (param->output_format) {
	case V4L2_PIX_FMT_H264:
		enc = rk_h264_encoder_alloc_ctx();
		break;
	case V4L2_PIX_FMT_VP8:
		enc = rk_vp8_encoder_alloc_ctx();
		break;
	default:
		VPU_PLG_ERR("Unsupported format set\n");
		return NULL;
	}

	if (enc == NULL) {
		VPU_PLG_ERR("Allocate encoder instance failed\n");
		return NULL;
	}

	retval = enc->ops->init(enc, param);
	if (retval < 0) {
		VPU_PLG_ERR("Encoder initialize failed\n");
		free(enc);
		return NULL;
	}
	return enc;
}

void rk_vepu_deinit(void *enc) {
	struct rk_venc *ienc = (struct rk_venc*)enc;

	assert(enc != NULL);
	ienc->ops->deinit(ienc);
	free(ienc);
}

int rk_vepu_get_config(void *enc, size_t *num_ctrls, uint32_t **ctrl_ids,
		       void ***payloads, uint32_t **payload_sizes)
{
	int retval;
	struct rk_venc *ienc = (struct rk_venc*)enc;
	int i;

	assert(enc != NULL && num_ctrls != NULL && ctrl_ids != NULL);
	assert(payloads != NULL && payload_sizes != NULL);

	retval = ienc->ops->before_encode(ienc);
	if (retval < 0) {
		VPU_PLG_ERR("Generate configuration failed\n");
		return -1;
	}

	ienc->ops->get_payloads(ienc, num_ctrls,
				ctrl_ids, payloads, payload_sizes);

	VPU_PLG_DBG("num_ctrls %d\n", *num_ctrls);

	for (i = 0; i < *num_ctrls; i++)
		VPU_PLG_DBG("ctrl_ids[%d] = 0x%x, payload_sizes[%d] = %u\n",
			    i, (*ctrl_ids)[i], i, (*payload_sizes)[i]);

	return 0;
}

int rk_vepu_update_config(void *enc, void *config, uint32_t config_size,
		uint32_t buffer_size) {
	int retval;
	struct rk_venc *ienc = (struct rk_venc*)enc;

	assert(enc != NULL && config != NULL);

	retval = ienc->ops->update_priv(ienc, config, config_size);
	if (retval < 0) {
		VPU_PLG_ERR("Update encoder private data failed\n");
		return -1;
	}
	return ienc->ops->after_encode(ienc, buffer_size);
}

int rk_vepu_update_param(void *enc,
		struct rk_vepu_runtime_param *param) {
	struct rk_venc *ienc = (struct rk_venc*)enc;

	assert(enc != NULL && param != NULL);

	if (param->bitrate != 0)
		ienc->runtime_param.bitrate = param->bitrate;

	if (param->framerate_numer != 0)
		ienc->runtime_param.framerate_numer = param->framerate_numer;

	if (param->framerate_denom != 0)
		ienc->runtime_param.framerate_denom = param->framerate_denom;

	if (param->keyframe_request != 0)
		ienc->runtime_param.keyframe_request = param->keyframe_request;

	ienc->ops->apply_param(ienc);
	return 0;
}

int rk_vepu_assemble_bitstream(void *enc, int fd, struct v4l2_buffer *buffer) {
  struct rk_venc *ienc = (struct rk_venc*)enc;

  assert(enc != NULL && buffer != NULL);
  if (ienc->ops->assemble_bitstream)
    return ienc->ops->assemble_bitstream(ienc, fd, buffer);

  return 0;
}
