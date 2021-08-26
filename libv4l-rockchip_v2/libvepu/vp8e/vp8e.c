/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

#include "vp8e.h"
#include "vp8e_bitstream.h"
#include "../rk_vepu_debug.h"

const int32_t vp8e_qp_tbl[2][11] = {
	{ 47, 57, 73, 93, 122, 155, 214, 294, 373, 506, 0x7FFFFFFF },
	{ 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20}};

static void vp8e_init_rc(struct rk_venc *ictx)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;
	struct v4l2_plugin_rate_control *rc = &ictx->rc;

	rc->pic_rc_en = true;
	rc->fps_num = 30;
	rc->fps_denom = 1;
	rc->vb.bit_rate = 1000000;
	rc->vb.actual_bits = 0;
	rc->vb.time_scale = rc->fps_num;
	rc->vb.virt_bits_cnt = 0;
	rc->vb.bucket_fullness = 0;
	rc->vb.pic_time_inc = 0;
	rc->gop_len = 150;
	rc->qp_min = 4;
	rc->qp_max = 127;
	rc->mb_per_pic = MB_COUNT(ctx->width) * MB_COUNT(ctx->height);
	rc->intra_qp_delta = -3;
	rc->qp = -1;
	rc->initiated = false;

	rk_venc_init_pic_rc(&ctx->venc.rc, vp8e_qp_tbl);
}

static int vp8e_init(struct rk_venc *ictx,
	struct rk_vepu_init_param *param)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;

	ictx->fmt = ENC_FORMAT_VP8;

	ctx->width = param->width;
	ctx->height = param->height;
	ctx->frm_in_gop = 0;

	ctx->hw_info.intra_frm_delta = 2;
	ctx->hw_info.last_frm_delta = 0;
	ctx->hw_info.golden_frm_delta = -2;
	ctx->hw_info.altref_frm_delta = -2;

	ctx->hw_info.bpred_mode_delta = 4;
	ctx->hw_info.zero_mode_delta = -2;
	ctx->hw_info.newmv_mode_delta = 2;
	ctx->hw_info.splitmv_mode_delta = 4;

	ctx->refresh_entropy = 1;
	ctx->frame_cnt = 0;
	ctx->last_frm_intra = false;

	ctx->hw_info.filter_sharpness = 0;
	ctx->hw_info.filter_level = 26; /* 0 ~ 63 */

	vp8e_init_rc(ictx);

	ctx->priv_data = (struct vp8_hw_privdata *)calloc(1, sizeof(*ctx->priv_data));
	if (ctx->priv_data == NULL) {
		VPU_PLG_ERR("allocate private data buffer failed\n");
		return -1;
	}

	ctx->rk_ctrl_ids[0] = V4L2_CID_PRIVATE_ROCKCHIP_HEADER;
	ctx->rk_ctrl_ids[1] = V4L2_CID_PRIVATE_ROCKCHIP_REG_PARAMS;
	ctx->rk_ctrl_ids[2] = V4L2_CID_PRIVATE_ROCKCHIP_HW_PARAMS;

	return 0;
}

static void vp8e_deinit(struct rk_venc *ictx)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;

	free(ctx->priv_data);
}

static int vp8e_begin_picture(struct rk_venc *ictx)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;
	struct rk3399_vp8e_reg_params *hw_info = &ctx->hw_info;
	int time_inc = 1;
	enum FRAME_TYPE frmtype = INTER_FRAME;

	VPU_PLG_ENTER();

	if (ictx->runtime_param.keyframe_request) {
		VPU_PLG_INF("got key frame request\n");
		ctx->frm_in_gop = 0;
		ictx->runtime_param.keyframe_request = 0;
	}

	hw_info->is_intra = 0;
	hw_info->filter_level = 28;
	if (ctx->frm_in_gop == 0) {
		hw_info->is_intra = 1;
		hw_info->filter_level = 48;
		time_inc = 0;
		frmtype = INTRA_FRAME;
	}

	rk_venc_before_pic_rc(&ctx->venc.rc, time_inc, frmtype);

	hw_info->qp = ictx->rc.qp;

	prepare_prob(&ctx->probs);

	vp8_pack_bitstream(ctx);

	hw_info->frm_hdr_size = ctx->hdr_len;
	hw_info->bool_enc_value = ctx->writer.lowvalue;
	hw_info->bool_enc_value_bits = 24 + ctx->writer.count;
	hw_info->bool_enc_range = ctx->writer.range;
	memcpy(hw_info->mv_prob, ctx->probs.mv, sizeof(ctx->probs.mv));
	hw_info->intra_prob = ctx->probs.intra;

	ctx->rk_payloads[0] = ctx->frmhdr;
	ctx->rk_payload_sizes[0] = FRAME_HEADER_SIZE;

	ctx->rk_payloads[1] = hw_info;
	ctx->rk_payload_sizes[1] = sizeof(struct rockchip_reg_params);

	ctx->rk_payloads[2] = ctx->priv_data;
	ctx->rk_payload_sizes[2] = sizeof(*ctx->priv_data);

	VPU_PLG_LEAVE();

	return 0;
}

static int vp8e_end_picture(struct rk_venc *ictx,
	 uint32_t outputStreamSize)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;

	VPU_PLG_ENTER();

	rk_venc_after_pic_rc(&ctx->venc.rc, outputStreamSize);

	ctx->last_frm_intra = ctx->hw_info.is_intra;

	ctx->frm_in_gop++;
	ctx->frm_in_gop %= ictx->rc.gop_len;

	ctx->frame_cnt++;

	VPU_PLG_LEAVE();

	return 0;
}

static int vp8e_update_priv(struct rk_venc *ictx, void *config, uint32_t cfglen)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;

	assert(ctx);
	assert(config);

	memcpy(&ctx->count, config, cfglen);

	return 0;
}

static void vp8e_apply_param(struct rk_venc *ictx)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;
	struct rk_vepu_runtime_param *param = &ictx->runtime_param;
	bool reinit = false;

	assert(ctx);

	if (param->bitrate != 0) {
		ctx->venc.rc.vb.bit_rate = param->bitrate;
		reinit = true;
	}

	if (param->framerate_numer != 0 && param->framerate_denom != 0) {
		ctx->venc.rc.fps_num = param->framerate_numer;
		ctx->venc.rc.fps_denom = param->framerate_denom;
		ictx->rc.vb.time_scale = param->framerate_numer;

		reinit = true;
	}

	if (reinit) {
		if (!ictx->rc.initiated) {
			ictx->rc.qp = -1;
			rk_venc_init_pic_rc(&ictx->rc, vp8e_qp_tbl);
			ictx->rc.initiated = true;
		} else {
			rk_venc_recalc_parameter(&ictx->rc);
		}
	}
}

static void vp8e_get_payloads(struct rk_venc *ictx, size_t *num, uint32_t **ids,
	void ***payloads, uint32_t **payload_sizes)
{
	struct rk_vp8_encoder *ctx = (struct rk_vp8_encoder *)ictx;

	VPU_PLG_ENTER();

	*num = VP8E_NUM_CTRLS;
	*ids = ctx->rk_ctrl_ids;
	*payloads = ctx->rk_payloads;
	*payload_sizes = ctx->rk_payload_sizes;

	VPU_PLG_LEAVE();
}

static struct rk_venc_ops vp8_enc_ops = {
	.init = vp8e_init,
	.before_encode = vp8e_begin_picture,
	.after_encode = vp8e_end_picture,
	.deinit = vp8e_deinit,
	.update_priv = vp8e_update_priv,
	.apply_param = vp8e_apply_param,
	.get_payloads = vp8e_get_payloads,
};

struct rk_venc* rk_vp8_encoder_alloc_ctx(void)
{
	struct rk_venc* enc =
		(struct rk_venc *)calloc(1, sizeof(struct rk_vp8_encoder));

	if (enc == NULL) {
		return NULL;
	}

	enc->ops = &vp8_enc_ops;

	return enc;
}
