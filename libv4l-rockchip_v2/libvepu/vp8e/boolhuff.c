/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "boolhuff.h"

void vp8_start_encode(BOOL_CODER *br, unsigned char *source)
{
	br->lowvalue = 0;
	br->range = 255;
	br->value = 0;
	br->count = -24;
	br->buffer = source;
	br->pos = 0;
}

void vp8_stop_encode(BOOL_CODER *br)
{
	int i;

	for (i = 0; i < 32; i++)
		vp8_encode_bool(br, 0, 128);
}

static const unsigned int norm[256] =
{
	0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void vp8_encode_bool(BOOL_CODER *br, int bit, int probability)
{
	unsigned int split;
	int count = br->count;
	unsigned int range = br->range;
	unsigned int lowvalue = br->lowvalue;
	register unsigned int shift;

	split = 1 + (((range - 1) * probability) >> 8);

	range = split;

	if (bit) {
		lowvalue += split;
		range = br->range - split;
	}

	shift = norm[range];

	range <<= shift;
	count += shift;

	if (count >= 0) {
		int offset = shift - count;

		if ((lowvalue << (offset - 1)) & 0x80000000) {
			int x = br->pos - 1;

			while (x >= 0 && br->buffer[x] == 0xff) {
				br->buffer[x] = (unsigned char)0;
				x--;
			}

			br->buffer[x] += 1;
		}

		br->buffer[br->pos++] = (lowvalue >> (24 - offset));
		lowvalue <<= offset;
		shift = count;
		lowvalue &= 0xffffff;
		count -= 8 ;
	}

	lowvalue <<= shift;
	br->count = count;
	br->lowvalue = lowvalue;
	br->range = range;
}

void vp8_encode_value(BOOL_CODER *br, int data, int bits)
{
	int bit;

	for (bit = bits - 1; bit >= 0; bit--)
		vp8_encode_bool(br, (1 & (data >> bit)), 0x80);
}
