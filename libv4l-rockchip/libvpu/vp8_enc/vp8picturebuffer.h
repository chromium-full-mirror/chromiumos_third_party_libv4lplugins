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

#ifndef _VP8PICTURE_BUFFER_H_
#define _VP8PICTURE_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>

#include "vp8entropytools.h"
#include "encasiccontroller.h"

#define BUFFER_SIZE 3

typedef struct {
  int32_t lumWidth;       /* Width of *lum */
  int32_t lumHeight;      /* Height of *lum */
  int32_t chWidth;        /* Width of *cb and *cr */
  int32_t chHeight;       /* Height of *cb and *cr */
  uint32_t lum;
  uint32_t cb;
} picture;

typedef struct refPic {
  picture picture;    /* Image data */
  entropy* entropy;   /* Entropy store of picture */
  int32_t poc;        /* Picture order count */

  bool i_frame;       /* I frame (key frame), only intra mb */
  bool p_frame;       /* P frame, intra and inter mb */
  bool show;      /* Frame is for display (showFrame flag) */
  bool ipf;       /* Frame is immediately previous frame */
  bool arf;       /* Frame is altref frame */
  bool grf;       /* Frame is golden frame */
  bool search;        /* Frame is used for motion estimation */
  struct refPic* refPic;  /* Back reference pointer to itself */
} refPic;

typedef struct {
  int32_t size;       /* Amount of allocated reference pictures */
  picture input;      /* Input picture */
  refPic refPic[BUFFER_SIZE + 1]; /* Reference picture store */
  refPic refPicList[BUFFER_SIZE]; /* Reference picture list */
  refPic* cur_pic;    /* Pointer to picture under reconstruction */
  refPic* last_pic;   /* Last picture */
} picBuffer;

/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/
void PictureBufferAlloc(picBuffer* picBuffer, int32_t width, int32_t height);
void PictureBufferFree(picBuffer* picBuffer);
void InitializePictureBuffer(picBuffer* picBuffer);
void UpdatePictureBuffer(picBuffer* picBuffer);
void PictureBufferSetRef(picBuffer* picBuffer, asicData_s* asic);

#endif
