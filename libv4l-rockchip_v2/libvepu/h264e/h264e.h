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

#ifndef _V4L2_PLUGIN_RK_H264E_H_
#define _V4L2_PLUGIN_RK_H264E_H_

#include <stdbool.h>
#include <stdint.h>

#include "../rk_vepu_interface.h"

#include "h264e_common.h"
#include "h264e_rate_control.h"

#include "../common/rk_venc.h"

#define H264E_NUM_CTRLS	1
#define H264E_MAX_STREAM_HEADER_SIZE 256

struct rk_h264_encoder {
	struct rk_venc venc;

	struct v4l2_plugin_h264_sps sps;
	struct v4l2_plugin_h264_pps pps;
	struct v4l2_plugin_h264_slice_param slice;
	struct rk3288_h264e_reg_params hw_info;
	struct v4l2_plugin_h264_feedback feedback;

	struct h264_mb_rate_control mbrc;

	char stream_header[H264E_MAX_STREAM_HEADER_SIZE];
	int stream_header_size;
	int h264_sps_pps_before_idr;

	int width;
	int height;

	int slice_size_mb_rows;
	int h264_inter4x4_disabled;

	int frm_in_gop;

	uint32_t rk_ctrl_ids[H264E_NUM_CTRLS];
	void *rk_payloads[H264E_NUM_CTRLS];
	uint32_t rk_payload_sizes[H264E_NUM_CTRLS];
};

struct rk_venc* rk_h264_encoder_alloc_ctx(void);
void rk_h264_encoder_free_ctx(struct rk_h264_encoder *enc);

#endif
