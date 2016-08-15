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

#ifndef _V4L2_PLUGIN_RK_VP8E_H_
#define _V4L2_PLUGIN_RK_VP8E_H_

#include <stdbool.h>
#include <stdint.h>

#include "../rk_vepu_interface.h"

#include "vp8e_prob_adapt.h"
#include "boolhuff.h"

#include "../common/rk_venc.h"

#define FRAME_HEADER_SIZE 1280

#define VP8E_NUM_CTRLS	3

#define VP8_PRIV_DATA_HEADER			192
#define VP8_PROBS_CTX_SIZE			((55 + 96) << 3)
#define VP8_SEGMENT_MAP_SIZE			4087//((1920 * 1088 * 4 / 256 + 63) / 64 * 8)

struct vp8_hw_privdata {
	uint8_t hdr[VP8_PRIV_DATA_HEADER];
	struct vp8_probs_hw probs_hw;
	uint8_t segmap[VP8_SEGMENT_MAP_SIZE];
};

struct rk_vp8_encoder {
	struct rk_venc venc;

	uint8_t frmhdr[FRAME_HEADER_SIZE];
	uint32_t hdr_len;

	uint8_t hw_prob_table[1208];
	struct vp8_probs probs;
	struct prob_count count;

	struct vp8_hw_privdata *priv_data;

	int frm_in_gop;
	int width;
	int height;

	int frame_cnt;
	bool last_frm_intra;

	bool refresh_entropy;

	bool default_coeff_prob_flag;   /* Flag for coeffProb == defaultCoeffProb */
	bool update_coeff_prob_flag;    /* Flag for coeffProb != oldCoeffProb */

	BOOL_CODER writer;

	struct rk3399_vp8e_reg_params hw_info;

	uint32_t rk_ctrl_ids[VP8E_NUM_CTRLS];
	void *rk_payloads[VP8E_NUM_CTRLS];
	uint32_t rk_payload_sizes[VP8E_NUM_CTRLS];
};

struct rk_venc* rk_vp8_encoder_alloc_ctx(void);

#endif
