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

#ifndef _V4L2_PLUGIN_RK_VENC_RATE_CONTROL_H_
#define _V4L2_PLUGIN_RK_VENC_RATE_CONTROL_H_

#include <inttypes.h>
#include <stdbool.h>

#define STATISTIC_TABLE_LENGTH		10

enum FRAME_TYPE {
	INTRA_FRAME = 0,
	INTER_FRAME = 1
};

struct bits_statistic {
	int32_t  bits[STATISTIC_TABLE_LENGTH];
	int32_t  pos;
	int32_t  len;
};

#define LINEAR_MODEL_STATISTIC_COUNT	15

struct linear_model {
	int32_t n;			/* elements count */
	int32_t i;			/* elements index for store */

	int64_t b;			/* coefficient */
	int64_t a;

	int64_t qp[LINEAR_MODEL_STATISTIC_COUNT];	/* x */
	int64_t r[LINEAR_MODEL_STATISTIC_COUNT];
	int64_t y[LINEAR_MODEL_STATISTIC_COUNT];	/* y = qp*qp*r */

	int32_t qp_last;		/* qp value in last calculate */
};

/* Virtual buffer */
struct virt_buffer {
	int32_t bit_rate;
	int32_t bits_per_pic;
	int32_t pic_time_inc;
	int32_t time_scale;
	int32_t virt_bits_cnt;
	int32_t actual_bits;
	int32_t bucket_fullness;
	int32_t gop_rem;
	int32_t window_rem;
};

struct v4l2_plugin_rate_control {
	bool pic_rc_en;
	int32_t mb_per_pic;
	enum FRAME_TYPE cur_frmtype;
	enum FRAME_TYPE pre_frmtype;
	int32_t qp_fixed;
	int32_t qp;
	int32_t qp_min;
	int32_t qp_max;
	int32_t qp_last;
	int32_t fps_num;
	int32_t fps_denom;
	struct virt_buffer vb;
	struct bits_statistic pid_inter;
	int32_t target_bits;
	int32_t acc_inter_qp;
	int32_t acc_inter_cnt;
	int32_t gop_len;
	int32_t intra_qp_delta;

	struct bits_statistic intra;
	struct bits_statistic pid_intra;
	struct bits_statistic gop;

	struct linear_model intra_frames;
	struct linear_model inter_frames;

	/* accumulate bits count for current gop */
	int32_t acc_bits_cnt;

	/* bitrate window which tries to match target */
	int32_t window_len;
	int32_t intra_interval;
	int32_t intra_interval_ctrl;

	const int32_t *qstep;
	int32_t qstep_size;

	bool initiated;
};

void rk_venc_recalc_parameter(struct v4l2_plugin_rate_control *rc);
bool rk_venc_init_pic_rc(struct v4l2_plugin_rate_control *rc,
	const int32_t qp_tbl[2][11]);
void rk_venc_after_pic_rc(struct v4l2_plugin_rate_control *rc,
	uint32_t byteCnt);
void rk_venc_before_pic_rc(struct v4l2_plugin_rate_control *rc,
	uint32_t timeInc, enum FRAME_TYPE frmtype);

#endif
