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

#ifndef _V4L2_PLUGIN_RK_VENC_H_
#define _V4L2_PLUGIN_RK_VENC_H_

#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>

#include "../rk_vepu_interface.h"

#include "rk_venc_rate_control.h"

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;

typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;

#define ABS(x)          ((x) < (0) ? -(x) : (x))
#define MAX(a, b)       ((a) > (b) ?  (a) : (b))
#define MIN(a, b)       ((a) < (b) ?  (a) : (b))
#define SIGN(a)         ((a) < (0) ? (-1) : (1))
#define CLIP3(v, min, max)  ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))
#define MB_COUNT(x)	(((x) + 15) >> 4)
#define DIV(a, b)       ((b) ? ((a) + (SIGN(a) * (b)) / 2) / (b) : (a))

#define ALIGN(x, a)      (((x) + (a) - 1) & ~((a) - 1))

#define container_of(ptr, type, member) ({      \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})

struct rk_venc;

struct rk_venc_ops {
	int (*init)(struct rk_venc *enc, struct rk_vepu_init_param *enc_parms);
	int (*before_encode)(struct rk_venc *enc);
	int (*after_encode)(struct rk_venc *enc, uint32_t outputStreamSize);
	void (*deinit)(struct rk_venc *enc);
	int (*update_priv)(struct rk_venc *enc, void *config, uint32_t len);
	void (*apply_param)(struct rk_venc *enc);
	void (*get_payloads)(struct rk_venc *enc, size_t *num, uint32_t **ids,
			     void ***payloads, uint32_t **payload_sizes);
	int (*assemble_bitstream)(struct rk_venc *enc, int fd,
				  struct v4l2_buffer *buffer);
};

enum ENC_FORMAT {
	ENC_FORMAT_H264,
	ENC_FORMAT_VP8
};

struct rk_venc {
	struct rk_venc_ops *ops;
	struct rk_vepu_runtime_param runtime_param;

	struct v4l2_plugin_rate_control rc;
	enum ENC_FORMAT	fmt;
};

/**
 * struct rk3288_vp8e_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct rk3288_vp8e_reg_params {
	u32 unused_00[5];
	u32 hdr_len;
	u32 unused_18[8];
	u32 enc_ctrl;
	u32 unused_3c;
	u32 enc_ctrl0;
	u32 enc_ctrl1;
	u32 enc_ctrl2;
	u32 enc_ctrl3;
	u32 enc_ctrl5;
	u32 enc_ctrl4;
	u32 str_hdr_rem_msb;
	u32 str_hdr_rem_lsb;
	u32 unused_60;
	u32 mad_ctrl;
	u32 unused_68;
	u32 qp_val[8];
	u32 bool_enc;
	u32 vp8_ctrl0;
	u32 rlc_ctrl;
	u32 mb_ctrl;
	u32 unused_9c[14];
	u32 rgb_yuv_coeff[2];
	u32 rgb_mask_msb;
	u32 intra_area_ctrl;
	u32 cir_intra_ctrl;
	u32 unused_e8[2];
	u32 first_roi_area;
	u32 second_roi_area;
	u32 mvc_ctrl;
	u32 unused_fc;
	u32 intra_penalty[7];
	u32 unused_11c;
	u32 seg_qp[24];
	u32 dmv_4p_1p_penalty[32];
	u32 dmv_qpel_penalty[32];
	u32 vp8_ctrl1;
	u32 bit_cost_golden;
	u32 loop_flt_delta[2];
};

/**
 * struct rk3288_h264e_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct rk3288_h264e_reg_params {
	u32 frame_coding_type;
	s32 pic_init_qp;
	s32 slice_alpha_offset;
	s32 slice_beta_offset;
	s32 chroma_qp_index_offset;
	s32 filter_disable;
	u16 idr_pic_id;
	s32 pps_id;
	s32 frame_num;
	s32 slice_size_mb_rows;
	s32 h264_inter4x4_disabled;
	s32 enable_cabac;
	s32 transform8x8_mode;
	s32 cabac_init_idc;

	/* rate control relevant */
	s32 qp;
	s32 mad_qp_delta;
	s32 mad_threshold;
	s32 qp_min;
	s32 qp_max;
	s32 cp_distance_mbs;
	s32 cp_target[10];
	s32 target_error[7];
	s32 delta_qp[7];
};

/**
 * struct rk3399_vp8e_reg_params - low level encoding parameters
 * TODO: Create abstract structures for more generic controls or just
 *       remove unused fields.
 */
struct rk3399_vp8e_reg_params {
	u32 is_intra;
	u32 frm_hdr_size;

	u32 qp;

	s32 mv_prob[2][19];
	s32 intra_prob;

	u32 bool_enc_value;
	u32 bool_enc_value_bits;
	u32 bool_enc_range;

	u32 filterDisable;
	u32 filter_sharpness;
	u32 filter_level;

	s32 intra_frm_delta;
	s32 last_frm_delta;
	s32 golden_frm_delta;
	s32 altref_frm_delta;

	s32 bpred_mode_delta;
	s32 zero_mode_delta;
	s32 newmv_mode_delta;
	s32 splitmv_mode_delta;
};

/**
 * struct rockchip_reg_params - low level encoding parameters
 */
struct rockchip_reg_params {
	/* Mode-specific data. */
	union {
		const struct rk3288_h264e_reg_params rk3288_h264e;
		const struct rk3288_vp8e_reg_params rk3288_vp8e;
		const struct rk3399_vp8e_reg_params rk3399_vp8e;
	};
};

#endif
