/*
 * Copyright 2016 Rockchip Electronics Co. LTD
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

#include "rk_venc_rate_control.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>

#include "rk_venc.h"
#include "libvepu/rk_vepu_debug.h"

#define I32_MAX			2147483647 /* 2 ^ 31 - 1 */
#define QP_DELTA		4
#define QP_DELTA_LIMIT		10
#define DRIFT_MAX		0x1FFFFFFF
#define DRIFT_MIN		-0x1FFFFFFF

static const int32_t h264_q_step[] = {
	3, 3, 3, 4, 4, 5, 5, 6, 7, 7,
	8, 9, 10, 11, 13, 14, 16, 18, 20, 23,
	25, 28, 32, 36, 40, 45, 51, 57, 64, 72,
	80, 90, 101, 114, 128, 144, 160, 180, 203, 228,
	256, 288, 320, 360, 405, 456, 513, 577, 640, 720,
	810, 896
};

#define QINDEX_RANGE	128
static const int32_t vp8_ac_lookup[QINDEX_RANGE] = {
	4,   5,   6,   7,   8,   9,   10,  11,  12,  13,
	14,  15,  16,  17,  18,  19,  20,  21,  22,  23,
	24,  25,  26,  27,  28,  29,  30,  31,  32,  33,
	34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
	44,  45,  46,  47,  48,  49,  50,  51,  52,  53,
	54,  55,  56,  57,  58,  60,  62,  64,  66,  68,
	70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
	90,  92,  94,  96,  98,  100, 102, 104, 106, 108,
	110, 112, 114, 116, 119, 122, 125, 128, 131, 134,
	137, 140, 143, 146, 149, 152, 155, 158, 161, 164,
	167, 170, 173, 177, 181, 185, 189, 193, 197, 201,
	205, 209, 213, 217, 221, 225, 229, 234, 239, 245,
	249, 254, 259, 264, 269, 274, 279, 284
};

static int32_t get_gop_avg_qp(struct v4l2_plugin_rate_control *rc)
{
	int32_t qp_aver = rc->qp_last;

	if (rc->acc_inter_qp && rc->acc_inter_cnt)
		qp_aver = DIV(rc->acc_inter_qp, rc->acc_inter_cnt);

	rc->acc_inter_qp = 0;
	rc->acc_inter_cnt = 0;

	return qp_aver;
}

static int32_t get_avg_bits(struct bits_statistic *p, int32_t n)
{
	int32_t i;
	int32_t sum = 0;
	int32_t pos = p->pos;

	if (!p->len)
		return 0;

	if (n == -1 || n > p->len)
		n = p->len;

	i = n;
	while (i--) {
		if (pos)
			pos--;
		else
			pos = p->len - 1;
		sum += p->bits[pos];
		if (sum < 0) {
			return I32_MAX / (n - i);
		}
	}
	return DIV(sum, n);
}

static int32_t axb_div_c(int32_t a, int32_t b, int32_t c)
{
	uint32_t left = 32;
	uint32_t right = 0;
	uint32_t shift;
	int32_t sign = 1;
	int32_t tmp;

	if (a == 0 || b == 0)
		return 0;
	else if ((a * b / b) == a && c != 0)
		return (a * b / c);

	if (a < 0) {
		sign = -1;
		a = -a;
	}
	if (b < 0) {
		sign *= -1;
		b = -b;
	}
	if (c < 0) {
		sign *= -1;
		c = -c;
	}

	if (c == 0)
		return 0x7FFFFFFF * sign;

	if (b > a) {
		tmp = b;
		b = a;
		a = tmp;
	}

	for (--left; (((uint32_t)a << left) >> left) != (uint32_t)a; --left)
		;

	left--;

	while (((uint32_t)b >> right) > (uint32_t)c)
		right++;

	if (right > left) {
		return 0x7FFFFFFF * sign;
	} else {
		shift = left - right;
		return (int32_t)((((uint32_t)a << shift) /
				  (uint32_t)c * (uint32_t)b) >> shift) * sign;
	}
}

static inline void reset_statistic(struct bits_statistic *p)
{
	memset(p, 0, sizeof(*p));
}

static inline void reset_linear_model(struct linear_model *p, int32_t qp)
{
	memset(p, 0, sizeof(*p));
	p->qp_last = qp;
}

static void update_statitistic(struct bits_statistic *p, int32_t bits)
{
	const int32_t clen = STATISTIC_TABLE_LENGTH;

	p->bits[p->pos] = bits;

	if (++p->pos >= clen) {
		p->pos = 0;
	}
	if (p->len < clen) {
		p->len++;
	}
}

/*
 * store previous intra frame bits / gop bits, and used for next gop intra frame
 * bit compensation
 */
static void save_intra_frm_ratio(struct v4l2_plugin_rate_control *rc)
{
	if (rc->acc_bits_cnt) {
		int32_t intra_frm_ratio =
			axb_div_c(get_avg_bits(&rc->intra, 1),
				  rc->mb_per_pic, 256) * 100;
		intra_frm_ratio = DIV(intra_frm_ratio, rc->acc_bits_cnt);
		intra_frm_ratio = MIN(99, intra_frm_ratio);

		update_statitistic(&rc->gop, intra_frm_ratio);
	}
	rc->acc_bits_cnt = 0;
}

static void update_pid_ctrl(struct bits_statistic *p, int32_t bits)
{
	p->len = 3;

	p->bits[0] = bits - p->bits[2];		/* Derivative */
	if ((bits > 0) && (bits + p->bits[1] > p->bits[1]))
		p->bits[1] = bits + p->bits[1];	/* Integral */
	if ((bits < 0) && (bits + p->bits[1] < p->bits[1]))
		p->bits[1] = bits + p->bits[1];	/* Integral */
	p->bits[2] = bits;			/* Proportional */
	VPU_PLG_DBG("P %d I %d D %d\n", p->bits[2], p->bits[1], p->bits[0]);
}

static inline int32_t get_pid_ctrl_value(struct bits_statistic *p)
{
	return DIV(p->bits[2] * 40 + p->bits[1] * 60 + p->bits[0] * 1, 1000);
}

/*
 * according to linear formula 'R * Q * Q = b * Q + a'
 * now give the target R, calculate a Qp value using
 * approximation
 */
static int32_t calculate_qp_using_linear_model(
	struct v4l2_plugin_rate_control *rc,
	struct linear_model *model,
	int64_t r)
{
	int32_t qp = model->qp_last;
	int64_t estimate_r = 0;
	int64_t diff = 0;
	int64_t diff_min = I32_MAX;
	int64_t qp_best = qp;
	int32_t tmp;

	VPU_PLG_DBG("a %lld b %lld\n", model->a, model->b);

	if (model->b == 0 && model->a == 0) {
		return qp_best;
	}

	if (r <= 0) {
		qp = CLIP3(qp_best + QP_DELTA, rc->qp_min, rc->qp_max);
		return qp;
	}

	do {
		int64_t qstep = rc->qstep[qp];
		estimate_r =
			DIV(model->b, qstep) + DIV(model->a, qstep * qstep);
		diff = estimate_r - r;
		if (ABS(diff) < diff_min) {
			diff_min = ABS(diff);
			qp_best = qp;
			if (diff > 0) {
				qp++;
			} else {
				qp--;
			}
		} else {
			break;
		}
	} while (qp <= rc->qp_max && qp >= rc->qp_min);

	tmp = qp_best - model->qp_last;
	if (tmp > QP_DELTA) {
		qp_best = model->qp_last + QP_DELTA;
		/*
		 * when there is a bit gap between requirement and actual bits,
		 * delta qp cannot quickly catch the requirement.
		 */
		if (tmp > QP_DELTA_LIMIT)
			qp_best = model->qp_last + QP_DELTA * 2;
	} else if (tmp < -QP_DELTA) {
		qp_best = model->qp_last - QP_DELTA;
	}

	model->qp_last = qp_best;

	return qp_best;
}

/* determine qp for current picture */
void calculate_pic_qp(struct v4l2_plugin_rate_control *rc)
{
	int32_t target_bits;
	int32_t norm_bits;

	if (rc->pic_rc_en != true) {
		rc->qp = rc->qp_fixed;
		return;
	}

	if (rc->cur_frmtype == INTRA_FRAME) {
		/*
		 * when there are no intra statistic information, we calcuate
		 * intra qp using previous gop inter frame average qp.
		 */
		rc->qp = get_gop_avg_qp(rc);
		save_intra_frm_ratio(rc);
		/*
		 * if all frames are intra we calculate qp
		 * using intra frame statistic info.
		 */
		if (rc->pre_frmtype == INTRA_FRAME) {
			target_bits = rc->target_bits -
				get_pid_ctrl_value(&rc->pid_intra);

			norm_bits = axb_div_c(target_bits, 256, rc->mb_per_pic);
			rc->qp = calculate_qp_using_linear_model(rc,
								 &rc->intra_frames,
								 norm_bits);
		}
	} else {
		/*
		 * calculate qp by matching to previous
		 * inter frames R-Q curve
		 */
		target_bits = rc->target_bits -
			get_pid_ctrl_value(&rc->pid_inter);

		norm_bits = axb_div_c(target_bits, 256, rc->mb_per_pic);
		rc->qp = calculate_qp_using_linear_model(rc, &rc->inter_frames,
							 norm_bits);
	}
}

static void store_linear_x_y(struct linear_model *model, int32_t r, int32_t qstep)
{
	model->qp[model->i] = qstep;
	model->r[model->i] = r;
	model->y[model->i] = r * qstep * qstep;

	model->n++;
	model->n = MIN(model->n, LINEAR_MODEL_STATISTIC_COUNT);

	model->i++;
	model->i %= LINEAR_MODEL_STATISTIC_COUNT;
}

/*
 * This function want to calculate coefficient 'b' 'a' using ordinary
 * least square.
 * y = b * x + a
 * b_n = accumulate(x * y) - n * (average(x) * average(y))
 * a_n = accumulate(x * x) * accumulate(y) - accumulate(x) * accumulate(x * y)
 * denom = accumulate(x * x) - n * (square(average(x))
 * b = b_n / denom
 * a = a_n / denom
 */
static void calculate_linear_coefficient(struct linear_model *model)
{
	int i = 0;
	int n;
	int64_t acc_xy = 0;
	int64_t acc_x = 0;
	int64_t acc_y = 0;
	int64_t acc_sq_x = 0;

	int64_t b_num = 0;
	int64_t denom = 0;

	int64_t *x = model->qp;
	int64_t *y = model->y;

	n = model->n;
	i = n;

	while (i--) {
		acc_xy += x[i] * y[i];
		acc_x += x[i];
		acc_y += y[i];
		acc_sq_x += x[i] * x[i];
	}

	b_num = n * acc_xy - acc_x * acc_y;
	denom = n * acc_sq_x - acc_x * acc_x;

	model->b = DIV(b_num, denom);
	model->a = DIV(acc_y, n) - DIV(acc_x * model->b, n);
}

/*
 * in the beginning of rate control, we should get a estimate qp value using
 * experience point.
 */
static int32_t caluate_qp_by_bits_est(int32_t bits, int32_t pels,
	const int32_t qp_tbl[2][11])
{
	const int32_t upscale = 8000;
	int32_t i = -1;

	/* prevents overflow */
	if (bits > 1000000)
		return qp_tbl[1][10];

	/* make room for multiplication */
	pels >>= 8;
	bits >>= 5;

	/* adjust the bits value for the current resolution */
	bits *= pels + 250;
	assert(pels > 0);
	assert(bits > 0);
	bits /= 350 + (3 * pels) / 4;
	bits = axb_div_c(bits, upscale, pels << 6);

	while (qp_tbl[0][++i] < bits);

	return qp_tbl[1][i];
}

static int32_t get_drift_bits(struct virt_buffer *vb,
	int32_t time_inc)
{
	int32_t drift, target;

	/*
	 * saturate actual_bits, this is to prevent overflows caused by much
	 * greater bitrate setting than is really possible to reach.
	 */
	vb->actual_bits = CLIP3(vb->actual_bits, DRIFT_MIN, DRIFT_MAX);

	vb->pic_time_inc += time_inc;
	vb->virt_bits_cnt += axb_div_c(vb->bit_rate, time_inc, vb->time_scale);
	target = vb->virt_bits_cnt - vb->actual_bits;

	/* saturate target, prevents rc going totally out of control.
	   This situation should never happen. */
	target = CLIP3(target, DRIFT_MIN, DRIFT_MAX);

	/* picture time inc must be in range of [0, time_scale) */
	while (vb->pic_time_inc >= vb->time_scale) {
		vb->pic_time_inc -= vb->time_scale;
		vb->virt_bits_cnt -= vb->bit_rate;
		vb->actual_bits -= vb->bit_rate;
	}

	drift = axb_div_c(vb->bit_rate, vb->pic_time_inc, vb->time_scale);
	drift -= vb->virt_bits_cnt;
	vb->virt_bits_cnt += drift;

	return target;
}

void rk_venc_recalc_parameter(struct v4l2_plugin_rate_control *rc)
{
	rc->vb.bits_per_pic = axb_div_c(rc->vb.bit_rate,
					rc->fps_denom, rc->fps_num);
}

bool rk_venc_init_pic_rc(struct v4l2_plugin_rate_control *rc,
	const int32_t qp_tbl[2][11])
{
	struct rk_venc *enc = container_of(rc, struct rk_venc, rc);
	struct virt_buffer *vb = &rc->vb;

	switch (enc->fmt) {
	case ENC_FORMAT_H264:
		rc->qstep = h264_q_step;
		rc->qstep_size = sizeof(h264_q_step) / sizeof(int32_t);
		break;
	case ENC_FORMAT_VP8:
		rc->qstep = vp8_ac_lookup;
		rc->qstep_size = sizeof(vp8_ac_lookup) / sizeof(int32_t);
		break;
	default:
		VPU_PLG_ERR("unsupport encoder format %d\n", (int)enc->fmt);
		return false;
	}

	if (rc->qp == -1) {
		int32_t tmp = axb_div_c(vb->bit_rate, rc->fps_denom, rc->fps_num);
		rc->qp = caluate_qp_by_bits_est(tmp, rc->mb_per_pic * 16 * 16, qp_tbl);
	}

	rc->qp = CLIP3(rc->qp, rc->qp_min, rc->qp_max);

	rc->cur_frmtype = INTRA_FRAME;
	rc->pre_frmtype = INTER_FRAME;

	rc->qp_last = rc->qp;
	rc->qp_fixed = rc->qp;

	vb->bits_per_pic = axb_div_c(vb->bit_rate, rc->fps_denom, rc->fps_num);

	reset_statistic(&rc->pid_inter);
	reset_statistic(&rc->pid_intra);
	reset_statistic(&rc->intra);
	reset_statistic(&rc->gop);

	reset_linear_model(&rc->intra_frames, rc->qp);
	reset_linear_model(&rc->inter_frames, rc->qp);

	rc->acc_inter_qp = 0;
	rc->acc_inter_cnt = 0;
	rc->acc_bits_cnt = 0;

	rc->window_len = rc->gop_len;
	vb->window_rem = rc->gop_len;
	rc->intra_interval_ctrl = rc->intra_interval = rc->gop_len;
	rc->target_bits = 0;

	return true;
}

void rk_venc_after_pic_rc(struct v4l2_plugin_rate_control *rc,
	uint32_t bytes)
{
	struct virt_buffer *vb = &rc->vb;
	int32_t bits = (int32_t)bytes * 8;
	int32_t norm_bits = 0;

	VPU_PLG_INF("get actual bits %d\n", bits);

	rc->acc_bits_cnt += bits;

	/* store the error between target and actual frame size */
	if (rc->cur_frmtype != INTRA_FRAME) {
		/* saturate the error to avoid inter frames with
		 * mostly intra MBs to affect too much */
		update_pid_ctrl(&rc->pid_inter,
			MIN(bits - rc->target_bits, 2 * rc->target_bits));
	} else {
		update_pid_ctrl(&rc->pid_intra, bits - rc->target_bits);
	}

	norm_bits = axb_div_c(bits, 256, rc->mb_per_pic);

	/* update number of bits used for residual, inter or intra */
	if (rc->cur_frmtype != INTRA_FRAME) {
		store_linear_x_y(&rc->inter_frames, norm_bits, rc->qstep[rc->qp]);
		calculate_linear_coefficient(&rc->inter_frames);
	} else {
		update_statitistic(&rc->intra, norm_bits);

		store_linear_x_y(&rc->intra_frames, norm_bits, rc->qstep[rc->qp]);
		calculate_linear_coefficient(&rc->intra_frames);
	}

	vb->bucket_fullness += bits;
	vb->actual_bits += bits;
}

void rk_venc_before_pic_rc(struct v4l2_plugin_rate_control *rc,
	uint32_t timeInc, enum FRAME_TYPE frmtype)
{
	struct virt_buffer *vb = &rc->vb;
	int32_t rcWindow, intraBits = 0, tmp = 0;

	rc->cur_frmtype = frmtype;

	tmp = get_drift_bits(&rc->vb, (int32_t)timeInc);

	if (vb->window_rem == 0) {
		vb->window_rem = rc->window_len - 1;
		reset_statistic(&rc->pid_inter);
		if (rc->cur_frmtype != rc->pre_frmtype)
			reset_statistic(&rc->pid_intra);
	} else {
		vb->window_rem--;
	}

	if (rc->cur_frmtype != INTRA_FRAME &&
		rc->intra_interval > 1) {
		intraBits = vb->bits_per_pic * rc->intra_interval *
			get_avg_bits(&rc->gop, 10) / 100;
		intraBits -= vb->bits_per_pic;
		intraBits /= (rc->intra_interval - 1);
		intraBits = MAX(0, intraBits);
	}

	/* Compensate for intra "stealing" bits from inters. */
	tmp += intraBits * (rc->intra_interval - rc->intra_interval_ctrl);

	rcWindow = MAX(1, rc->window_len);
	rc->target_bits = vb->bits_per_pic - intraBits + DIV(tmp, rcWindow);
	rc->target_bits = MAX(0, rc->target_bits);

	VPU_PLG_INF("require target bits %d\n", rc->target_bits);
	calculate_pic_qp(rc);

	rc->qp = CLIP3(rc->qp, rc->qp_min, rc->qp_max);
	rc->qp_last = rc->qp;

	if (rc->cur_frmtype == INTRA_FRAME) {
		/*
		 * if there is not a all intra coding, we prefer a better intra
		 * frame to get a better psnr.
		 */
		if (rc->pre_frmtype != INTRA_FRAME)
			rc->qp += rc->intra_qp_delta;

		rc->qp = CLIP3(rc->qp, rc->qp_min, rc->qp_max);
		if (rc->intra_interval_ctrl > 1)
			rc->intra_interval = rc->intra_interval_ctrl;
		rc->intra_interval_ctrl = 1;
	} else {
		rc->acc_inter_qp += rc->qp;
		rc->acc_inter_cnt++;
		rc->intra_interval_ctrl++;

		if (rc->intra_interval_ctrl > rc->intra_interval)
			rc->intra_interval = rc->intra_interval_ctrl;
	}

	rc->pre_frmtype = rc->cur_frmtype;

	VPU_PLG_INF("get qp %d\n", rc->qp);
}
