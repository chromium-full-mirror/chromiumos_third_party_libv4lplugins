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

#include "vp8e_bitstream.h"

#include <memory.h>

#include "../rk_vepu_debug.h"
#include "boolhuff.h"

typedef BOOL_CODER vp8_writer;

#define vp8_write vp8_encode_bool
#define vp8_write_literal vp8_encode_value
#define vp8_write_bit(W, V) vp8_write(W, V, 0x80)

static void pack_mv_prob(vp8_writer *bc, int32_t curr[2][19], int32_t prev[2][19]) {
	int32_t i, j;
	int32_t prob, new, old;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 19; j++) {
			prob = mv_update_prob[i][j];
			old = prev[i][j];
			new = curr[i][j];

			if (new == old) {
				vp8_write(bc, 0, prob);
			} else {
				vp8_write(bc, 1, prob);
				vp8_write_literal(bc, new >> 1, 7);
			}
		}
	}
}

static void pack_coeff_prob(vp8_writer* bc, int32_t curr[4][8][3][11],
		int32_t prev[4][8][3][11]) {
	int32_t i, j, k, l;
	int32_t prob, new_prob, old_prob;

	VPU_PLG_ENTER();

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 3; k++) {
				for (l = 0; l < 11; l++) {
					prob = coeff_update_prob[i][j][k][l];
					old_prob = prev[i][j][k][l];
					new_prob = curr[i][j][k][l];

					if (new_prob == old_prob) {
						vp8_write(bc, 0, prob);
					} else {
						vp8_write(bc, 1, prob);
						vp8_write_literal(bc, new_prob, 8);
					}
				}
			}
		}
	}

	VPU_PLG_LEAVE();
}

static void vp8_pack_ref_mb_delta(vp8_writer *bc, int32_t delta)
{
	if (delta != 0) {
		vp8_write_bit(bc, 1);
		vp8_write_literal(bc, ABS(delta), 6);
		vp8_write_bit(bc, delta < 0);
	} else {
		vp8_write_bit(bc, 0);
	}
}

#define FRAME_SHOW	1
#define FRAME_PROFILE	1
void vp8_pack_bitstream(struct rk_vp8_encoder *ctx)
{
	struct vp8_probs *entropy = &ctx->probs;
	vp8_writer *bc = &ctx->writer;  /* "Frame header" buffer */
	int frm_tag_len = ctx->frm_in_gop == 0 ? 10 : 3;
	int32_t tmp;
	int idx = 0;

	vp8_start_encode(bc, ctx->frmhdr + frm_tag_len);

	/* Color space and pixel Type (Key frames only) */
	if (ctx->frm_in_gop == 0) {
		/* color space type */
		vp8_write_bit(bc, 0);

		/* clamping type */
		vp8_write_bit(bc, 0);
	}

	/* segmentation flag, disable segmentation by default
	 * if roi or intra area enabled, segmentation need to
	 * be configured
	 */
	vp8_write_bit(bc, 0);

	/* filter type, USING NORMAL filter type by default */
	vp8_write_bit(bc, 0);

	/* filter level, can be set to 0 ~ 63*/
	vp8_write_literal(bc, ctx->hw_info.filter_level, 6);

	/* filter sharpness level */
	vp8_write_literal(bc, ctx->hw_info.filter_sharpness, 3);

	/* Loop filter adjustments */
	vp8_write_bit(bc, 1);
	/* Filter level delta references reset by key frame */
	/* following code present only on filter adjustment enabled */
	/* we are fix the loop filter delta adjustment, so only update
	   when intra frame account */
	if (ctx->frm_in_gop == 0) {
		/* Do the deltas need to be updated */
		vp8_write_bit(bc, 1);

		/* ref frame mode based deltas */
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.intra_frm_delta);
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.last_frm_delta);
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.golden_frm_delta);
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.altref_frm_delta);
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.bpred_mode_delta);
		/* mb mode based deltas */
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.zero_mode_delta);
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.newmv_mode_delta);
		vp8_pack_ref_mb_delta(bc, ctx->hw_info.splitmv_mode_delta);
	} else {
		/* Do the deltas need to be updated */
		vp8_write_bit(bc, 0);
	}

	/* token partition, support 0 or 1, so there are two
	   dct partions supported, we use only 1 here */
	vp8_write_literal(bc, 0, 2);

	/* YacQi quantizer index */
	vp8_write_literal(bc, ctx->hw_info.qp, 7);

	/* TODO: delta quantization index, we disabled here */
	/* YdcDelta flag */
	vp8_write_bit(bc, 0);

	/* Y2dcDelta flag */
	vp8_write_bit(bc, 0);

	/* Y2acDelta flag */
	vp8_write_bit(bc, 0);

	/* UVdecDelta flag */
	vp8_write_bit(bc, 0);

	/* UVacDelta flag */
	vp8_write_bit(bc, 0);

	/* Update grf and arf buffers and sing bias, see decodframe.c 863.
	* TODO swaping arg->grf and grf->arf in the same time is not working
	* because of bug in the libvpx? */
	if (ctx->frm_in_gop != 0) {
		/* Input picture after reconstruction is set to new grf/arf */
		vp8_write_bit(bc, 0); /* Grf refresh */
		vp8_write_bit(bc, 0); /* Arf refresh */

		vp8_write_literal(bc, 0, 2);    /* Not updated */

		vp8_write_literal(bc, 0, 2);    /* Not updated */

		/* sign bias, do not using golden or altref here, set them to false */
		vp8_write_bit(bc, 0);  /* Grf */
		vp8_write_bit(bc, 0);  /* Arf */
	}


	/* Refresh entropy probs flag,
	   if 0 -> put default proabilities.
	   If 1 -> use previous frame probabilities */
	vp8_write_bit(bc, 1);

	/* ipf refresh last frame flag. Note that key frame always updates ipf */
	if (ctx->frm_in_gop != 0)
		vp8_write_bit(bc, 1);

	/* Coeff probabilities, TODO: real updates */
	pack_coeff_prob(bc, entropy->coeff, entropy->last_coeff);

	/*  mb_no_coeff_skip . This flag indicates at the frame level if
	*  skipping of macroblocks with no non-zero coefficients is enabled.
	*  If it is set to 0 then prob_skip_false is not read and
	*  mb_skip_coeff is forced to 0 for all macroblocks (see Sections 11.1
	*  and 12.1). TODO  */
	vp8_write_bit(bc, 1);

	/* Probability used for decoding noCoeff flag, depens above flag TODO*/
	vp8_write_literal(bc, entropy->skip_false, 8);

	if (ctx->frm_in_gop == 0)
		goto tag_write;

	/* The rest are inter frame only */

	/* Macroblock is intra predicted probability */
	vp8_write_literal(bc, entropy->intra, 8);

	/* Inter is predicted from immediately previous frame probability */
	vp8_write_literal(bc, entropy->last_prob, 8);

	/* Inter is predicted from golden frame probability */
	vp8_write_literal(bc, entropy->gf_prob, 8);

	/* Intra mode probability updates not supported yet TODO */
	vp8_write_bit(bc, 0);

	/* Intra chroma probability updates not supported yet TODO */
	vp8_write_bit(bc, 0);

	/* Motion vector probability update not supported yet TOTO real updates */
	pack_mv_prob(bc, entropy->mv, entropy->last_mv);

tag_write:
	/* Frame tag contains (lsb first):
	* 1. A 1-bit frame type (0 for key frames, 1 for inter frames)
	* 2. A 3-bit version number (0 - 3 are defined as 4 different profiles
	* 3. A 1-bit showFrame flag (1 when current frame is display)
	* 4. A 19-bit size of the first data partition in bytes
	* Note that frame tag is written to the stream in little endian mode */

	tmp = ((ctx->writer.pos) << 5) |
		((FRAME_SHOW ? 1 : 0) << 4) |
		(FRAME_PROFILE << 1) |
		(ctx->frm_in_gop != 0);

	/* Note that frame tag is written _really_ literal to buffer, don't use
	* vp8_write_literal() use VP8PutBit() instead */

	ctx->frmhdr[idx++] = tmp & 0xff;
	ctx->frmhdr[idx++] = (tmp >> 8) & 0xff;
	ctx->frmhdr[idx++] = (tmp >> 16) & 0xff;

	if (ctx->frm_in_gop != 0)
		goto ret;

	/* For key frames this is followed by a further 7 bytes of uncompressed
	* data as follows */
	ctx->frmhdr[idx++] = 0x9d;
	ctx->frmhdr[idx++] = 0x01;
	ctx->frmhdr[idx++] = 0x2a;

	ctx->frmhdr[idx++] = ctx->width & 0xff;
	ctx->frmhdr[idx++] = ctx->width >> 8;

	ctx->frmhdr[idx++] = ctx->height & 0xff;
	ctx->frmhdr[idx++] = ctx->height >> 8;

ret:
	ctx->hdr_len = frm_tag_len + bc->pos;
}

