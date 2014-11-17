/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
*/

#ifndef VP8PUT_BITS_H
#define VP8PUT_BITS_H

#include <stdint.h>

typedef struct {
  uint8_t* data;       /* Pointer to next byte of data buffer */
  uint8_t* pData;      /* Pointer to beginning of data buffer */
  int32_t size;        /* Size of *data in bytes */
  int32_t byteCnt;     /* Data buffer stream byte count */

  int32_t range;       /* Bool encoder range [128, 255] */
  int32_t bottom;      /* Bool encoder left endpoint */
  int32_t bitsLeft;    /* Bool encoder bits left before flush bottom */
} vp8buffer;

typedef struct {
  int32_t value;       /* Bits describe the bool tree  */
  int32_t number;      /* Number, valid bit count in above tree */
  int32_t index[9];    /* Probability table index */
} tree;

int32_t VP8SetBuffer(vp8buffer*, uint8_t*, int32_t);
void VP8PutByte(vp8buffer* buffer, int32_t byte);
void VP8PutLit(vp8buffer*, int32_t, int32_t);
void VP8PutBool(vp8buffer* buffer, int32_t prob, int32_t boolValue);
void VP8PutTree(vp8buffer* buffer, tree const* tree, int32_t* prob);
void VP8FlushBuffer(vp8buffer* buffer);

#endif
