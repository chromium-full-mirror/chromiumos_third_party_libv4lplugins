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

#ifndef H264_RATE_CONTROL_H
#define H264_RATE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "../common/rk_venc_rate_control.h"

#define CTRL_LEVELS          7
#define CHECK_POINTS_MAX    10

struct mb_qpctrl {
	int32_t word_error[CTRL_LEVELS]; /* Check point error bit */
	int32_t qp_delta[CTRL_LEVELS];  /* Check point qp difference */
	int32_t word_cnt_target[CHECK_POINTS_MAX];    /* Required bit count */
	int32_t word_cnt_prev[CHECK_POINTS_MAX];  /* Real bit count */
	int32_t chkptr_distance;
	int32_t check_points;
};

struct h264_mb_rate_control {
	bool mb_rc_en;
	int32_t bits_per_non_zero_coef;
	int32_t non_zero_cnt;
	struct mb_qpctrl qp_ctrl;
};

void h264e_before_mb_rate_control(struct h264_mb_rate_control *rc);
void h264e_after_mb_rate_control(struct h264_mb_rate_control *rc, uint32_t coded_bytes,
	uint32_t non_zero_cnt, uint32_t qp_sum);

#endif
