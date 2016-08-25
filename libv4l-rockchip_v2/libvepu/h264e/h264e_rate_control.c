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

#include <memory.h>
#include <assert.h>

#include "h264e.h"
#include "h264e_common.h"
#include "h264e_rate_control.h"
#include "../rk_vepu_debug.h"

#define WORD_CNT_MAX      65535

static void calculate_mb_model_using_linear_model(
	struct h264_mb_rate_control *rc,
	int32_t non_zero_target)
{
	struct rk_h264_encoder *enc =
		container_of(rc, struct rk_h264_encoder, mbrc);
	const int32_t sscale = 256;
	struct mb_qpctrl *qc = &rc->qp_ctrl;
	int32_t scaler;
	int32_t i;
	int32_t tmp;
	int32_t mb_per_pic = MB_COUNT(enc->width) * MB_COUNT(enc->height);
	int32_t chk_ptr_cnt = MIN(MB_COUNT(enc->height), CHECK_POINTS_MAX);
	int32_t chk_ptr_distance = mb_per_pic / (chk_ptr_cnt + 1);
	int32_t bits_per_pic = enc->venc.rc.vb.bits_per_pic;

	assert(non_zero_target < (0x7FFFFFFF / sscale));

	if(non_zero_target > 0) {
		/* scaler is non-zero coefficent count per macro-block
		   plus 256 */
		scaler = DIV(non_zero_target * sscale, mb_per_pic);
	} else {
		return;
	}

	for(i = 0; i < chk_ptr_cnt; i++) {
		/* tmp is non-zero coefficient count target for i-th
		   check point */
		tmp = (scaler * (chk_ptr_distance * (i + 1) + 1)) / sscale;
		tmp = MIN(WORD_CNT_MAX, tmp / 32 + 1);
		if (tmp < 0) tmp = WORD_CNT_MAX;    /* Detect overflow */
		qc->word_cnt_target[i] = tmp; /* div32 for regs */
	}

	/* calculate nz count for avg. bits per frame */
	/* tmp is target non-zero coefficient count for average size pic  */
	tmp = DIV(bits_per_pic * 256, rc->bits_per_non_zero_coef);

	/* ladder 'non-zero coefficent count target' - 'non-zero coefficient
	   actual' of check point */
	qc->word_error[0] = -tmp * 3;
	qc->qp_delta[0] = -3;
	qc->word_error[1] = -tmp * 2;
	qc->qp_delta[1] = -2;
	qc->word_error[2] = -tmp * 1;
	qc->qp_delta[2] = -1;
	qc->word_error[3] = tmp * 1;
	qc->qp_delta[3] = 0;
	qc->word_error[4] = tmp * 2;
	qc->qp_delta[4] = 1;
	qc->word_error[5] = tmp * 3;
	qc->qp_delta[5] = 2;
	qc->word_error[6] = tmp * 4;
	qc->qp_delta[6] = 3;

	for(i = 0; i < CTRL_LEVELS; i++)
		qc->word_error[i] = CLIP3(qc->word_error[i] / 4, -32768, 32767);
}

static void calculate_mb_model_using_adaptive_model(
	struct h264_mb_rate_control *rc,
	int32_t non_zero_target)
{
	struct rk_h264_encoder *enc =
		container_of(rc, struct rk_h264_encoder, mbrc);
	const int32_t sscale = 256;
	struct mb_qpctrl *qc = &rc->qp_ctrl;
	int32_t i;
	int32_t tmp;
	int32_t scaler;
	int32_t chk_ptr_cnt = MIN(MB_COUNT(enc->height), CHECK_POINTS_MAX);
	int32_t bits_per_pic = enc->venc.rc.vb.bits_per_pic;

	assert(non_zero_target < (0x7FFFFFFF / sscale));

	if((non_zero_target > 0) && (rc->non_zero_cnt > 0))
		scaler = DIV(non_zero_target * sscale, rc->non_zero_cnt);
	else
		return;

	for(i = 0; i < chk_ptr_cnt; i++) {
		tmp = (int32_t)(qc->word_cnt_prev[i] * scaler) / sscale;
		tmp = MIN(WORD_CNT_MAX, tmp / 32 + 1);
		if (tmp < 0) tmp = WORD_CNT_MAX;    /* Detect overflow */
		qc->word_cnt_target[i] = tmp; /* div32 for regs */
	}

	/* calculate nz count for avg. bits per frame */
	tmp = DIV(bits_per_pic * 256, (rc->bits_per_non_zero_coef * 3));

	qc->word_error[0] = -tmp * 3;
	qc->qp_delta[0] = -3;
	qc->word_error[1] = -tmp * 2;
	qc->qp_delta[1] = -2;
	qc->word_error[2] = -tmp * 1;
	qc->qp_delta[2] = -1;
	qc->word_error[3] = tmp * 1;
	qc->qp_delta[3] = 0;
	qc->word_error[4] = tmp * 2;
	qc->qp_delta[4] = 1;
	qc->word_error[5] = tmp * 3;
	qc->qp_delta[5] = 2;
	qc->word_error[6] = tmp * 4;
	qc->qp_delta[6] = 3;

	for(i = 0; i < CTRL_LEVELS; i++)
		qc->word_error[i] =
			CLIP3(qc->word_error[i] / 4, -32768, 32767);
}

static void calculate_mb_model(struct h264_mb_rate_control *rc,
	int32_t target_bits)
{
	struct rk_h264_encoder *enc =
		container_of(rc, struct rk_h264_encoder, mbrc);
	int32_t non_zero_target;
	int32_t mb_per_pic = MB_COUNT(enc->width) * MB_COUNT(enc->height);
	int32_t coeff_cnt_max = mb_per_pic * 24 * 16;

	/* Disable macroblock rate control for intra frame,
	   because coefficient target will be wrong */
	if(enc->frm_in_gop == 0 || rc->bits_per_non_zero_coef == 0)
		return;

	/* Required zero cnt */
	non_zero_target = DIV(target_bits * 256, rc->bits_per_non_zero_coef);
	non_zero_target = CLIP3(non_zero_target, 0, coeff_cnt_max);

	non_zero_target = MIN(0x7FFFFFFFU / 1024U, (uint32_t)non_zero_target);

	VPU_PLG_INF("mb rc target non-zero coefficient count %d\n",
		non_zero_target);

	/* Use linear model when previous frame can't be used for prediction */
	if (enc->frm_in_gop != 0 || rc->non_zero_cnt == 0)
		calculate_mb_model_using_linear_model(rc, non_zero_target);
	else
		calculate_mb_model_using_adaptive_model(rc, non_zero_target);
}

void h264e_before_mb_rate_control(struct h264_mb_rate_control *rc)
{
	struct rk_h264_encoder *enc =
		container_of(rc, struct rk_h264_encoder, mbrc);

	memset(rc->qp_ctrl.word_cnt_target, 0,
	       sizeof(rc->qp_ctrl.word_cnt_target));

	if (enc->venc.rc.cur_frmtype == INTER_FRAME &&
	    enc->venc.rc.pre_frmtype == INTER_FRAME &&
	    rc->mb_rc_en)
		calculate_mb_model(rc, enc->venc.rc.target_bits);
}

void h264e_after_mb_rate_control(struct h264_mb_rate_control *rc,
	uint32_t bytes, uint32_t non_zero_cnt, uint32_t qp_sum)
{
	struct rk_h264_encoder *enc = container_of(rc, struct rk_h264_encoder,
		mbrc);
	int32_t bits = bytes * 8;

	VPU_PLG_INF("mb rc get actual non-zero coefficient count %u\n",
		non_zero_cnt);

	if (enc->frm_in_gop != 0) {
		rc->bits_per_non_zero_coef = DIV(bits * 256, non_zero_cnt);
		rc->non_zero_cnt = non_zero_cnt;
	}
}
