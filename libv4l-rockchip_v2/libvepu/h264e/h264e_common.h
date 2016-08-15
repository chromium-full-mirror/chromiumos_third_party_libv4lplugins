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

#ifndef _V4L2_PLUGIN_RK_H264E_COMMON_H_
#define _V4L2_PLUGIN_RK_H264E_COMMON_H_

#include <stdbool.h>
#include <stdint.h>

#include "../common/rk_venc.h"

#define MB_PER_PIC(ctx)		\
	(ctx->sps.pic_width_in_mbs * ctx->sps.pic_height_in_map_units)

/* frame coding type defined by hardware */
#define FRAME_CODING_TYPE_INTER		0
#define FRAME_CODING_TYPE_INTRA		1

enum H264ENC_LEVEL {
	H264ENC_LEVEL_1 = 10,
	H264ENC_LEVEL_1_b = 99,
	H264ENC_LEVEL_1_1 = 11,
	H264ENC_LEVEL_1_2 = 12,
	H264ENC_LEVEL_1_3 = 13,
	H264ENC_LEVEL_2 = 20,
	H264ENC_LEVEL_2_1 = 21,
	H264ENC_LEVEL_2_2 = 22,
	H264ENC_LEVEL_3 = 30,
	H264ENC_LEVEL_3_1 = 31,
	H264ENC_LEVEL_3_2 = 32,
	H264ENC_LEVEL_4_0 = 40,
	H264ENC_LEVEL_4_1 = 41
};

enum sliceType_e {
	PSLICE = 0,
	ISLICE = 2,
	PSLICES = 5,
	ISLICES = 7
};

struct v4l2_plugin_h264_feedback {
	int32_t qpSum;
	int32_t cp[10];
	int32_t madCount;
	int32_t rlcCount;
};

struct v4l2_plugin_h264_sps {
	u8 profile_idc;
	u8 constraint_set0_flag :1;
	u8 constraint_set1_flag :1;
	u8 constraint_set2_flag :1;
	u8 constraint_set3_flag :1;
	u8 level_idc;
	u8 seq_parameter_set_id;
	u8 chroma_format_idc;
	u8 bit_depth_luma_minus8;
	u8 bit_depth_chroma_minus8;
	u8 qpprime_y_zero_transform_bypass_flag :1;
	u16 log2_max_frame_num_minus4;
	u8 pic_order_cnt_type;
	u8 max_num_ref_frames;
	u8 gaps_in_frame_num_value_allowed_flag :1;
	u16 pic_width_in_mbs;
	u16 pic_height_in_map_units;
	u8 frame_mbs_only_flag :1;
	u8 direct_8x8_inference_flag :1;
	u8 frame_cropping_flag :1;
	u32 frame_crop_left_offset;
	u32 frame_crop_right_offset;
	u32 frame_crop_top_offset;
	u32 frame_crop_bottom_offset;
	u8 vui_parameters_present_flag :1;
};

struct v4l2_plugin_h264_pps {
	u8 pic_parameter_set_id;
	u8 seq_parameter_set_id;
	u8 entropy_coding_mode_flag :1;
	u8 pic_order_present_flag :1;
	u8 num_slice_groups_minus_1;
	u8 num_ref_idx_l0_default_active_minus1;
	u8 num_ref_idx_l1_default_active_minus1;
	u8 weighted_pred_flag :1;
	u8 weighted_bipred_idc;
	s8 pic_init_qp_minus26;
	s8 pic_init_qs_minus26;
	s8 chroma_qp_index_offset;
	u8 deblocking_filter_control_present_flag :1;
	u8 constrained_intra_pred_flag :1;
	u8 redundant_pic_cnt_present_flag :1;
	u8 transform_8x8_mode_flag :1;
};

struct v4l2_plugin_h264_slice_param {
	u8 slice_type;
	u8 pic_parameter_set_id;
	u16 frame_num;
	u16 idr_pic_id;
	u8 cabac_init_idc;
	u8 disable_deblocking_filter_idc;
	s8 slice_alpha_c0_offset_div2;
	s8 slice_beta_offset_div2;
};

#endif
