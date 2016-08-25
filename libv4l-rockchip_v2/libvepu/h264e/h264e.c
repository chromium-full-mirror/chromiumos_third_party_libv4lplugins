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

#include "h264e.h"
#include "h264e_rate_control.h"
#include "../common/rk_venc_rate_control.h"
#include "../rk_vepu_debug.h"

const int32_t h264e_qp_tbl[2][11] = {
	{ 27, 44, 72, 119, 192, 314, 453, 653, 952, 1395, 0x7FFFFFFF },
	{ 51, 47, 43, 39, 35, 31, 27, 23, 19, 15, 11} };

static void h264e_init_sps(struct v4l2_plugin_h264_sps *sps)
{
	sps->profile_idc = 77;

	/* fixed values limited by hardware */
	sps->constraint_set0_flag = 1;
	sps->constraint_set1_flag = 1;
	sps->constraint_set2_flag = 1;

	/* if level idc == 1b, constraint_set3_flag need to be set true */
	sps->constraint_set3_flag = 0;

	/* if level max than 31, we prefer disable 4x4 mv mode
	   to limit max mv count per 2mb */
	sps->level_idc = H264ENC_LEVEL_4_0;
	sps->seq_parameter_set_id = 0;

	/* fixed values limited by hardware */
	sps->chroma_format_idc = 1;
	sps->bit_depth_luma_minus8 = 0;
	sps->bit_depth_chroma_minus8 = 0;
	sps->qpprime_y_zero_transform_bypass_flag = 0;
	sps->log2_max_frame_num_minus4 = 12;
	sps->pic_order_cnt_type = 2;

	sps->max_num_ref_frames = 1;
	sps->gaps_in_frame_num_value_allowed_flag = 0;
	sps->pic_width_in_mbs = 1280 / 16;
	sps->pic_height_in_map_units = 720 / 16;
	sps->frame_mbs_only_flag = 1;

	sps->direct_8x8_inference_flag = 1;
	sps->frame_cropping_flag = 0;

	sps->vui_parameters_present_flag = 1;
}

static void h264e_init_pps(struct v4l2_plugin_h264_pps *pps)
{
	pps->pic_parameter_set_id = 0;
	pps->seq_parameter_set_id = 0;

	/* enable cabac coding need set profile_idc large than 77 */
	pps->entropy_coding_mode_flag = 1;

	/* fixed value limited by hardware */
	pps->pic_order_present_flag = 0;

	pps->num_ref_idx_l0_default_active_minus1 = 0;
	pps->num_ref_idx_l1_default_active_minus1 = 0;

	/* fixed value limited by hardware */
	pps->weighted_pred_flag = 0;
	pps->weighted_bipred_idc = 0;

	pps->pic_init_qp_minus26 = 0;
	pps->pic_init_qs_minus26 = 0;
	pps->chroma_qp_index_offset = 2;
	pps->deblocking_filter_control_present_flag = 1;
	pps->constrained_intra_pred_flag = 0;
	pps->num_slice_groups_minus_1 = 0;

	/* fixed value limited by hardware */
	pps->redundant_pic_cnt_present_flag = 0;

	/* enable transform_8x8, need profile_idc large than 100 */
	pps->transform_8x8_mode_flag = 0;
}

static void h264e_init_slice(struct v4l2_plugin_h264_slice_param *slice)
{
	slice->slice_type = ISLICE;
	slice->pic_parameter_set_id = 0;
	slice->frame_num = 0;
	slice->idr_pic_id = -1;
	slice->cabac_init_idc = 0;
	slice->disable_deblocking_filter_idc = 0;
	slice->slice_alpha_c0_offset_div2 = 0;
	slice->slice_beta_offset_div2 = 0;
}

static void h264e_init_rc(struct rk_venc *ictx)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;
	struct mb_qpctrl *qpCtrl = &ctx->mbrc.qp_ctrl;
	struct v4l2_plugin_rate_control *rc = &ictx->rc;

	memset(qpCtrl, 0, sizeof(*qpCtrl));

	ctx->mbrc.mb_rc_en = true;
	qpCtrl->check_points = MIN(ctx->sps.pic_height_in_map_units - 1,
				  CHECK_POINTS_MAX);
	qpCtrl->chkptr_distance =
		MB_PER_PIC(ctx) / (qpCtrl->check_points + 1);

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
	rc->qp_min = 10;
	rc->qp_max = 51;
	rc->mb_per_pic = MB_PER_PIC(ctx);
	rc->intra_qp_delta = -3;
	rc->qp = -1;
	rc->initiated = false;

	rk_venc_init_pic_rc(&ctx->venc.rc, h264e_qp_tbl);
}

static int h264e_init(struct rk_venc *ictx,
	struct rk_vepu_init_param *param)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;

	ictx->fmt = ENC_FORMAT_H264;

	h264e_init_sps(&ctx->sps);
	h264e_init_pps(&ctx->pps);
	h264e_init_slice(&ctx->slice);

	ctx->width = param->width;
	ctx->height = param->height;
	ctx->slice_size_mb_rows = 0;
	ctx->frm_in_gop = 0;

	ctx->sps.pic_width_in_mbs = MB_COUNT(ctx->width);
	ctx->sps.pic_height_in_map_units = MB_COUNT(ctx->height);

	if (ctx->width != ctx->sps.pic_width_in_mbs * 16 ||
	    ctx->height != ctx->sps.pic_height_in_map_units * 16) {
		ctx->sps.frame_cropping_flag = 1;
		ctx->sps.frame_crop_right_offset =
			ctx->sps.pic_width_in_mbs * 16 - ctx->width;
		ctx->sps.frame_crop_bottom_offset =
			ctx->sps.pic_height_in_map_units * 16 - ctx->height;
	}

	h264e_init_rc(ictx);

	if (ctx->sps.level_idc >= H264ENC_LEVEL_3_1)
		ctx->h264_inter4x4_disabled = 1;
	else
		ctx->h264_inter4x4_disabled = 0;

	if (ctx->pps.transform_8x8_mode_flag == 2)
		ctx->pps.transform_8x8_mode_flag = 1;

	ctx->rk_ctrl_ids[0] = V4L2_CID_PRIVATE_ROCKCHIP_REG_PARAMS;

	return 0;
}

static void h264e_deinit(struct rk_venc *ictx)
{

}

static int h264e_begin_picture(struct rk_venc *ictx)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;
	int i;
	enum FRAME_TYPE frmtype = INTER_FRAME;
	int timeInc = 1;
	struct rk3288_h264e_reg_params *hw_info = &ctx->hw_info;

	if (ictx->runtime_param.keyframe_request) {
		VPU_PLG_INF("got key frame request\n");
		ctx->frm_in_gop = 0;
		ictx->runtime_param.keyframe_request = 0;
	}

	hw_info->frame_coding_type = FRAME_CODING_TYPE_INTER;
	if (ctx->frm_in_gop == 0) {
		ctx->slice.idr_pic_id++;
		ctx->slice.idr_pic_id %= 16;
		ctx->slice.frame_num = 0;
		hw_info->frame_coding_type = FRAME_CODING_TYPE_INTRA;
		frmtype = INTRA_FRAME;
		timeInc = 0;
	}

	rk_venc_before_pic_rc(&ctx->venc.rc, timeInc, frmtype);
	h264e_before_mb_rate_control(&ctx->mbrc);

	hw_info->pic_init_qp = ctx->pps.pic_init_qp_minus26 + 26;
	hw_info->transform8x8_mode = ctx->pps.transform_8x8_mode_flag;
	hw_info->enable_cabac = ctx->pps.entropy_coding_mode_flag != 0;

	hw_info->chroma_qp_index_offset = ctx->pps.chroma_qp_index_offset;
	hw_info->pps_id = ctx->pps.pic_parameter_set_id;

	hw_info->filter_disable = ctx->slice.disable_deblocking_filter_idc;
	hw_info->slice_alpha_offset = ctx->slice.slice_alpha_c0_offset_div2 * 2;
	hw_info->slice_beta_offset = ctx->slice.slice_beta_offset_div2 * 2;
	hw_info->idr_pic_id = ctx->slice.idr_pic_id;
	hw_info->frame_num = ctx->slice.frame_num;
	hw_info->slice_size_mb_rows = ctx->slice_size_mb_rows;
	hw_info->cabac_init_idc = ctx->slice.cabac_init_idc;

	hw_info->qp = ctx->venc.rc.qp;

	hw_info->mad_qp_delta = 0;
	hw_info->mad_threshold = 0;
	hw_info->qp_min = ctx->venc.rc.qp_min;
	hw_info->qp_max = ctx->venc.rc.qp_max;
	hw_info->cp_distance_mbs = ctx->mbrc.qp_ctrl.chkptr_distance;

	for (i = 0; i < ctx->mbrc.qp_ctrl.check_points; i++) {
		hw_info->cp_target[i] = ctx->mbrc.qp_ctrl.word_cnt_target[i];
	}

	for (i = 0; i < CTRL_LEVELS; i++) {
		hw_info->target_error[i] = ctx->mbrc.qp_ctrl.word_error[i];
		hw_info->delta_qp[i] = ctx->mbrc.qp_ctrl.qp_delta[i];
	}

	hw_info->h264_inter4x4_disabled = ctx->h264_inter4x4_disabled;

	ctx->rk_payloads[0] = hw_info;
	ctx->rk_payload_sizes[0] = sizeof(struct rockchip_reg_params);

	return 0;
}

static int h264e_end_picture(struct rk_venc *ictx,
	 uint32_t outputStreamSize)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;
	struct v4l2_plugin_h264_feedback *feedback = &ctx->feedback;
	int i;

	for (i = 0; i < CHECK_POINTS_MAX; i++)
		ctx->mbrc.qp_ctrl.word_cnt_prev[i] = feedback->cp[i];
	h264e_after_mb_rate_control(&ctx->mbrc, outputStreamSize,
				 feedback->rlcCount, feedback->qpSum);
	rk_venc_after_pic_rc(&ctx->venc.rc, outputStreamSize);

	ctx->frm_in_gop++;
	ctx->frm_in_gop %= ctx->venc.rc.gop_len;

	ctx->slice.frame_num++;
	ctx->slice.frame_num %=
		(1 << (ctx->sps.log2_max_frame_num_minus4 + 4));

	return 0;
}

static int h264e_update_priv(struct rk_venc *ictx, void *config, uint32_t cfglen)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;

	assert(ctx);
	assert(config);

	memcpy(&ctx->feedback, config, sizeof(ctx->feedback));

	return 0;
}

static void h264e_apply_param(struct rk_venc *ictx)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;
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
			rk_venc_init_pic_rc(&ictx->rc, h264e_qp_tbl);
			ictx->rc.initiated = true;
		} else {
			rk_venc_recalc_parameter(&ictx->rc);
		}
	}
}

static void h264e_get_payloads(struct rk_venc *ictx, size_t *num, uint32_t **ids,
	void ***payloads, uint32_t **payload_sizes)
{
	struct rk_h264_encoder *ctx = (struct rk_h264_encoder *)ictx;

	*num = H264E_NUM_CTRLS;
	*ids = ctx->rk_ctrl_ids;
	*payloads = ctx->rk_payloads;
	*payload_sizes = ctx->rk_payload_sizes;
}

static struct rk_venc_ops h264_enc_ops = {
	.init = h264e_init,
	.before_encode = h264e_begin_picture,
	.after_encode = h264e_end_picture,
	.deinit = h264e_deinit,
	.update_priv = h264e_update_priv,
	.apply_param = h264e_apply_param,
	.get_payloads = h264e_get_payloads,
};

struct rk_venc* rk_h264_encoder_alloc_ctx(void)
{
	struct rk_venc* enc =
		(struct rk_venc*)calloc(1, sizeof(struct rk_h264_encoder));

	if (enc == NULL) {
		return NULL;
	}

	enc->ops = &h264_enc_ops;

	return enc;
}

