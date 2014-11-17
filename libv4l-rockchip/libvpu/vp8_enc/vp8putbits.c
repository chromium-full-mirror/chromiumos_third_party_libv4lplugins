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

#include "vp8putbits.h"

#include <stdio.h>
#include "enccommon.h"

/*------------------------------------------------------------------------------
    SetBuffer
    Input   buffer  Pointer to the buffer structure.
        data    Pointer to data buffer.
        size    Size of data buffer.
    Return  ENCHW_OK    Buffer status is OK.
        ENCHW_NOK   Buffer overflow.
------------------------------------------------------------------------------*/
int32_t VP8SetBuffer(vp8buffer* buffer, uint8_t* data, int32_t size) {
  if ((buffer == NULL) || (data == NULL) || (size < 1)) return ENCHW_NOK;

  buffer->data = data;
  buffer->pData = data;       /* First position of buffer */
  buffer->size = size;        /* Buffer size in bytes */
  buffer->range = 255;
  buffer->bottom = 0;             /* PutBool bottom */
  buffer->bitsLeft = 24;
  buffer->byteCnt = 0;

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------
    PutByte write byte literallyt to next place of buffer and advance data
    pointer to next place

    Input   buffer      Pointer to the buffer stucture
            value       Byte
------------------------------------------------------------------------------*/
void VP8PutByte(vp8buffer* buffer, int32_t byte) {
  ASSERT((uint32_t)byte < 256);
  ASSERT(buffer->data < buffer->pData + buffer->size);
  *buffer->data++ = byte;
  buffer->byteCnt++;
}

/*------------------------------------------------------------------------------
    PutLit write "literal" bits to stream using PutBool() where probability
    is 128. Note that real bits written to stream are not necessarily same
    than literal value. Bit write order: MSB...LSB.
------------------------------------------------------------------------------*/
void VP8PutLit(vp8buffer* buffer, int32_t value, int32_t number) {
  ASSERT(number < 32 && number > 0);
  ASSERT(((value & (-1 << number)) == 0));

  while (number--) {
    VP8PutBool(buffer, 128, (value >> number) & 0x1);
  }
}

/*------------------------------------------------------------------------------
    PutBool
------------------------------------------------------------------------------*/
void VP8PutBool(vp8buffer* buffer, int32_t prob, int32_t boolValue) {
  int32_t split = 1 + ((buffer->range - 1) * prob >> 8);
  int32_t lengthBits = 0;
  int32_t bits = 0;

  if (boolValue) {
    buffer->bottom += split;
    buffer->range -= split;
  } else {
    buffer->range = split;
  }

  while (buffer->range < 128) {
    /* Detect carry and add carry bit to already written
     * buffer->data if needed */
    if (buffer->bottom < 0) {
      uint8_t* data = buffer->data;
      while (*--data == 255) {
        *data = 0;
      }
      (*data)++;
    }
    buffer->range <<= 1;
    buffer->bottom <<= 1;

    if (!--buffer->bitsLeft) {
      lengthBits += 8;
      bits <<= 8;
      bits |= (buffer->bottom >> 24) & 0xff;
      TRACE_BIT_STREAM(bits & 0xff, 8);
      *buffer->data++ = (buffer->bottom >> 24) & 0xff;
      buffer->byteCnt++;
      buffer->bottom &= 0xffffff;     /* Keep 3 bytes */
      buffer->bitsLeft = 8;
      /* TODO use big enough buffer and check buffer status
       * for example in the beginning of mb row */
      ASSERT(buffer->data < buffer->pData + buffer->size - 1);
    }
  }
}

/*------------------------------------------------------------------------------
    PutTree
------------------------------------------------------------------------------*/
void VP8PutTree(vp8buffer* buffer, tree const* tree, int32_t* prob) {
  int32_t value = tree->value;
  int32_t number = tree->number;
  int32_t const* index = tree->index;

  while (number--) {
    VP8PutBool(buffer, prob[*index++], (value >> number) & 1);
  }
}

/*------------------------------------------------------------------------------
    FlushBuffer put remaining buffer->bottom bits to the stream
------------------------------------------------------------------------------*/
void VP8FlushBuffer(vp8buffer* buffer) {
  int32_t bitsLeft = buffer->bitsLeft;
  int32_t bottom = buffer->bottom;

  /* Detect (unlikely) carry and add carry bit to already written
   * buffer->data if needed */
  if (bottom & (1 << (32 - bitsLeft))) {
    uint8_t* data = buffer->data;
    while (*--data == 255) {
      *data = 0;
    }
    (*data)++;
  }

  /* Move remaining bits to left until byte boundary */
  bottom <<= (bitsLeft & 0x7);

  /* Move remaining bytes to left until word boundary */
  bottom <<= (bitsLeft >> 3) * 8;

  /* Write valid (and possibly padded) bits to stream */
  *buffer->data++ = 0xff & (bottom >> 24);
  *buffer->data++ = 0xff & (bottom >> 16);
  *buffer->data++ = 0xff & (bottom >> 8);
  *buffer->data++ = 0xff & bottom;
  buffer->byteCnt += 4;

  TRACE_BIT_STREAM(bottom, 32);

  COMMENT("flush");

}

