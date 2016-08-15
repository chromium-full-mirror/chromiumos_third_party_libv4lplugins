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

#include "vp8e_prob_adapt.h"

#include <assert.h>
#include <memory.h>

#include "vp8e.h"
#include "../common/rk_venc.h"
#include "../rk_vepu_debug.h"

enum {
	MAX_BAND = 7,
	MAX_CTX  = 3,
};

static void swap_endianess(uint32_t *buf, uint32_t size)
{
	uint32_t i = 0;
	uint32_t dw = size / 4;

	assert((size % 8) == 0);

	while (dw > 0) {
		uint32_t tmp1 = 0;
		uint32_t tmp2 = 0;

		tmp1 |= (buf[i] & 0xFF) << 24;
		tmp1 |= (buf[i] & 0xFF00) << 8;
		tmp1 |= (buf[i] & 0xFF0000) >> 8;
		tmp1 |= (buf[i] & 0xFF000000) >> 24;

		tmp2 |= (buf[i + 1] & 0xFF) << 24;
		tmp2 |= (buf[i + 1] & 0xFF00) << 8;
		tmp2 |= (buf[i + 1] & 0xFF0000) >> 8;
		tmp2 |= (buf[i + 1] & 0xFF000000) >> 24;

		buf[i++] = tmp2;
		buf[i++] = tmp1;

		dw -= 2;
	}
}

static void pack_prob_table(struct vp8_probs* probs)
{
	struct rk_vp8_encoder *ctx =
		(struct rk_vp8_encoder *)container_of(probs,
			struct rk_vp8_encoder, probs);
	struct vp8_hw_privdata *priv_data = ctx->priv_data;
	struct vp8_probs_hw *probs_hw = &priv_data->probs_hw;
	int32_t i, j, k, l;

	VPU_PLG_ENTER();

	/* create probabilities table for hw */
	memset(probs_hw, 0, 56);

#define PROB_TRANS(x)	(probs_hw->x = probs->x)
	PROB_TRANS(skip_false);
	PROB_TRANS(intra);
	PROB_TRANS(last_prob);
	PROB_TRANS(gf_prob);
	PROB_TRANS(segment[0]);
	PROB_TRANS(segment[1]);
	PROB_TRANS(segment[2]);
	PROB_TRANS(y_mode[0]);
	PROB_TRANS(y_mode[1]);
	PROB_TRANS(y_mode[2]);
	PROB_TRANS(y_mode[3]);
	PROB_TRANS(uv_mode[0]);
	PROB_TRANS(uv_mode[1]);
	PROB_TRANS(uv_mode[2]);

	probs_hw->mv_short[0] = probs->mv[1][0];
	probs_hw->mv_short[1] = probs->mv[0][0];

	probs_hw->mv_sign[0] = probs->mv[1][1];
	probs_hw->mv_sign[1] = probs->mv[0][1];

	probs_hw->mv_x_8 = probs->mv[1][17];
	probs_hw->mv_x_9 = probs->mv[1][18];

	probs_hw->mv_y_8 = probs->mv[0][17];
	probs_hw->mv_y_9 = probs->mv[0][18];

	for (i = 0; i < 8; i++) {
		/* mv size */
		probs_hw->mv_x_size_9_16[i] = probs->mv[1][9 + i];
		probs_hw->mv_y_size_9_16[i] = probs->mv[0][9 + i];

		if (i == 7)
			break;

		/* mv short tree */
		probs_hw->mv_x_short_tree[i] = probs->mv[1][2 + i];
		probs_hw->mv_y_short_tree[i] = probs->mv[0][2 + i];
	}

	if (ctx->update_coeff_prob_flag) {
		/* DCT coeff probabilities 0-2, two fields per line. */
		for (i = 0; i < 4; i++)
			for (j = 0; j < 8; j++)
				for (k = 0; k < 3; k++) {
					for (l = 0; l < 3; l++) {
						probs_hw->coeff0[i][j][k][l] =
							probs->coeff[i][j][k][l];
					}
					probs_hw->coeff0[i][j][k][3] = 0;
				}

		/* second probability table in ext mem.
		* DCT coeff probabilities 4 5 6 7 8 9 10 3 on each line.
		* coeff 3 was moved from first table to second so it is last. */
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 8; j++) {
				for (k = 0; k < 3; k++) {
					for (l = 4; l < 11; l++)
						probs_hw->coeff1[i][j][k][l - 4] =
							probs->coeff[i][j][k][l];
					probs_hw->coeff1[i][j][k][7] =
						probs->coeff[i][j][k][3];
				}
			}
		}
	}

	if (ctx->update_coeff_prob_flag) {
		swap_endianess((uint32_t *)probs_hw, sizeof(*probs_hw));
	} else {
		swap_endianess((uint32_t *)probs_hw, 56);
	}

	VPU_PLG_LEAVE();
}

/*------------------------------------------------------------------------------
    update
    determine if given probability is to be updated (savings larger than
    cost of update)
------------------------------------------------------------------------------*/
static uint32_t update_prob_on_cost(uint32_t updP, uint32_t left, uint32_t right, uint32_t oldP,
	uint32_t newP, uint32_t fixed) {
	int32_t u, s;

	/* how much it costs to update a coeff */
	u = (int32_t)fixed + ((vp8_prob_cost[255 - updP] - vp8_prob_cost[updP]) >> 8);
	/* bit savings if updated */
	s = ((int32_t)left * /* zero branch count */
	/* diff cost for '0' bin */
	(vp8_prob_cost[oldP] - vp8_prob_cost[newP]) +
	(int32_t)right * /* one branch count */
	/* diff cost for '1' bin */
	(vp8_prob_cost[255 - oldP] - vp8_prob_cost[255 - newP])) >> 8;

	return (s > u);
}

static uint32_t calculate_prob(uint32_t left, uint32_t right, uint32_t last_prob) {
	uint32_t prob;

	if (left + right) {
		prob = (left * 255) / (left + right);
		prob &= -2;
		if (!prob)
			prob = 1;
	} else {
		prob = last_prob;
	}

	return prob;
}

/* In order to save the memory of count stored,
   hardware skip some coefficients without meaning */
const int32_t eob_token_index[4][MAX_BAND][MAX_CTX] = {
	{
		{-1, -1, -1}, {0, 1, 2}, {-1, 3, 4}, {-1, 5, 6},
		{-1, 7, 8}, {-1, 9, 10}, {-1, 11, 12}
	}, {
		{13, 14, 15}, {-1, 16, 17}, {-1, 18, 19}, {-1, 20, 21},
		{-1, 22, 23}, {-1, 24, 25}, {-1, 26, 27}
	}, {
		{28, 29, 30}, {-1, 31, 32}, {-1, 33, 34}, {-1, 35, 36},
		{-1, 37, 38}, {-1, 39, 40}, {-1, 41, 42}
	}, {
		{43, 44, 45}, {-1, 46, 47}, {-1, 48, 49}, {-1, 50, 51},
		{-1, 52, 53}, {-1, 54, 55}, {-1, 56, 57}
	}
};

const int32_t zero_peg_token_index[4][MAX_BAND][MAX_CTX] = {
	{
		{-1, -1, -1}, {0, 1, 2}, {3, 4, 5},
		{6, 7, 8}, {9, 10, 11},{12, 13, 14},{15, 16, 17}
	}, {
		{18, 19, 20}, {21, 22, 23}, {24, 25, 26},
		{27, 28, 29}, {30, 31, 32}, {33, 34, 35}, {36, 37, 38}
	}, {
		{39, 40, 41}, {42, 43, 44}, {45, 46, 47},
		{48, 49, 50}, {51, 52, 53}, {54, 55, 56}, {57, 58, 59}
	}, {
		{60, 61, 62}, {63, 64, 65}, {66, 67, 68},
		{69, 70, 71}, {72, 73, 74}, {75, 76, 77}, {78, 79, 80}
	}
};

/* adapt entropy using previous frame encoder output symbols count */
static void adapt_probs(struct vp8_probs *probs)
{
	struct rk_vp8_encoder *enc = container_of(probs, struct rk_vp8_encoder, probs);
	int32_t i, j, k;
	uint32_t prob, left, right, oldP, updP;
	uint32_t subtree0_count, subtree1_count;
	uint32_t subtree_count[2];

	struct prob_count *cnt_tbl = &enc->count;
	u32 mb_per_frame = MB_COUNT(enc->width) * MB_COUNT(enc->height);

	/* Update the HW prob table only when needed. */
	enc->update_coeff_prob_flag = false;

	/* Use default propabilities as reference when needed. */
	if (!enc->refresh_entropy || enc->frm_in_gop == 0) {
		/* Only do the copying when something has changed. */
		if (!enc->default_coeff_prob_flag) {
			memcpy(probs->coeff, defaultCoeffProb,
				sizeof(defaultCoeffProb));
			enc->update_coeff_prob_flag = true;
		}
		memcpy(probs->mv, defaultMvProb, sizeof(defaultMvProb));
		enc->default_coeff_prob_flag = 1;
	}

	/* store current probs */
	memcpy(probs->last_coeff, probs->coeff, sizeof(probs->coeff));
	if (enc->frame_cnt == 0 || !enc->last_frm_intra)
		memcpy(probs->last_mv, probs->mv, sizeof(probs->mv));

	/* init probs */
	probs->skip_false = defaultSkipFalseProb[enc->hw_info.qp];

	/* Do not update on first frame, token/branch counters not valid yet. */
	if (enc->frame_cnt == 0)
		return;

	for (i = 0; i < 4; i++) {
		/* All but last (==7) bands */
		for (j = 0; j < MAX_BAND; j++) {
			/* All three neighbour contexts */
			for (k = 0; k < MAX_CTX; k++) {
				/* last token of current (type,band,ctx) */

				/* caculate prob count index of other token */
				s32 index = zero_peg_token_index[i][j][k];

				right = index >= 0 ? cnt_tbl->token_other[index] : 0;

				/* probability about zero token */
				oldP = probs->coeff[i][j][k][1];
				updP = coeff_update_prob[i][j][k][1];

				/* caculate prob count index of zero token */
				index = zero_peg_token_index[i][j][k];

				left = index >= 0 ? cnt_tbl->token_zero[index] : 0;

				if (left + right) {
					prob = ((left * 256) + ((left + right) >> 1)) /
						(left + right);
					if (prob > 255) prob = 255;
				} else {
					prob = oldP;
				}

				if (update_prob_on_cost(updP, left, right, oldP, prob, 8)) {
					probs->coeff[i][j][k][1] = prob;
					enc->update_coeff_prob_flag = true;
				}
				right += left;

				oldP = probs->coeff[i][j][k][0];
				updP = coeff_update_prob[i][j][k][0];

				index = eob_token_index[i][j][k];

				left = index >= 0 ? cnt_tbl->token_eob[index] : 0;

				if (left + right) {
					prob = ((left * 256) + ((left + right) >> 1)) / (left + right);
					if (prob > 255) prob = 255;
				} else {
					prob = oldP;
				}

				if (update_prob_on_cost(updP, left, right, oldP, prob, 8)) {
					probs->coeff[i][j][k][0] = prob;
					enc->update_coeff_prob_flag = true;
				}
			}
		}
	}

	/* If updating coeffProbs the defaults are no longer in use. */
	if (enc->update_coeff_prob_flag)
		enc->default_coeff_prob_flag = false;

	/* skip prob */
	prob = cnt_tbl->skip_mb * 256 / mb_per_frame;
	probs->skip_false = CLIP3(256 - (int32_t)prob, 0, 255);

	/* intra prob, do not update if previous was intra frame,
	   set it to default value */
	probs->intra = 63;
	if (!enc->last_frm_intra) {
		prob = cnt_tbl->intra_mb * 255 / mb_per_frame;
		probs->intra = CLIP3(prob, 0, 255);
	}

	/* mv probs should not be updated if previous or current frame is intra,
	   no mv information in intra frame */
	if (enc->last_frm_intra || enc->frm_in_gop == 0)
		return;

	/* mv probs i = 0 for mv vertical component
	   i = 1 for mv horizontal component */
	/* see vp8 document 17. Motion Vector Decoding*/
	for (i = 0; i < 2; i++) {
		left = cnt_tbl->mv[i].short_mv;
		right = cnt_tbl->mv[i].long_mv;

		prob = calculate_prob(left, right, probs->last_mv[i][0]);
		if (update_prob_on_cost(mv_update_prob[i][0], left, right,
			probs->last_mv[i][0], prob, 6))
			probs->mv[i][0] = prob;

		/* sign prob */
		right += left; /* total mvs */
		left = cnt_tbl->mv[i].mv_sign; /* positive mvs count */
		/* amount of negative vectors = total - positive - zero vectors */
		right -= (left + cnt_tbl->mv[i].mv[0]);

		prob = calculate_prob(left, right, probs->last_mv[i][1]);
		if (update_prob_on_cost(mv_update_prob[i][1], left, right,
			probs->last_mv[i][1], prob, 6))
			probs->mv[i][1] = prob;

		/* short mv probs, sub tree 00 and 01 (0/1 and 2/3) */
		for (j = 0; j < 2; j++) {
			left = cnt_tbl->mv[i].mv[j * 2];
			right = cnt_tbl->mv[i].mv[j * 2 + 1];
			prob = calculate_prob(left, right, probs->last_mv[i][4 + j]);
			if (update_prob_on_cost(mv_update_prob[i][4 + j], left, right,
				probs->last_mv[i][4+j], prob, 6))
				probs->mv[i][4 + j] = prob;
			/* count for subtree 00/01 */
			subtree_count[j] = left + right;
		}
		/* short mv probs, subtree 0 */
		prob = calculate_prob(subtree_count[0], subtree_count[1], probs->last_mv[i][3]);
		if (update_prob_on_cost(mv_update_prob[i][3], subtree_count[0], subtree_count[1],
			probs->last_mv[i][3], prob, 6))
			probs->mv[i][3] = prob;

		/* total count for subtree 0 = sum of count subtree 00 and subtree 01 */
		subtree0_count = subtree_count[0] + subtree_count[1];

		/* short mv probs, subtree 10 and 11 (4/5 and 6/7) */
		for (j = 0; j < 2; j++) {
			left = cnt_tbl->mv[i].mv[j * 2 + 4];
			right = cnt_tbl->mv[i].mv[j * 2 + 5];

			prob = calculate_prob(left, right, probs->last_mv[i][7 + j]);
			if (update_prob_on_cost(mv_update_prob[i][7 + j], left, right,
				probs->last_mv[i][7 + j], prob, 6))
				probs->mv[i][7 + j] = prob;
			/* count for subtree 10/11 */
			subtree_count[j] = left + right;
		}
		/* short mv probs, subtree 1 */
		prob = calculate_prob(subtree_count[0], subtree_count[1], probs->last_mv[i][6]);
		if (update_prob_on_cost(mv_update_prob[i][6], subtree_count[0], subtree_count[1],
			probs->last_mv[i][6], prob, 6))
			probs->mv[i][6] = prob;

		subtree1_count = subtree_count[0] + subtree_count[1];

		/* short mv probs, root tree */
		prob = calculate_prob(subtree0_count, subtree1_count,
			probs->last_mv[i][2]);
		if (update_prob_on_cost(mv_update_prob[i][2], subtree0_count, subtree1_count,
			probs->last_mv[i][2], prob, 6))
			probs->mv[i][2] = prob;
	}
}

void prepare_prob(struct vp8_probs *entropy)
{
	assert(sizeof(defaultCoeffProb) == sizeof(entropy->coeff));
	assert(sizeof(defaultCoeffProb) == sizeof(coeff_update_prob));
	assert(sizeof(defaultMvProb) == sizeof(mv_update_prob));
	assert(sizeof(defaultMvProb) == sizeof(entropy->mv));

	adapt_probs(entropy);

	/* Default propability */
	entropy->last_prob = 255;	/* Stetson-Harrison method TODO */
	entropy->gf_prob = 128;		/* Stetson-Harrison method TODO */
	memcpy(entropy->y_mode, YmodeProb, sizeof(YmodeProb));
	memcpy(entropy->uv_mode, UVmodeProb, sizeof(UVmodeProb));

	pack_prob_table(entropy);
}

