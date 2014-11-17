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

#ifndef __VP8_INSTANCE_H__
#define __VP8_INSTANCE_H__

#include "encasiccontroller.h"
#include "enccommon.h"
#include "vp8seqparameterset.h"
#include "vp8picparameterset.h"
#include "vp8picturebuffer.h"
#include "vp8putbits.h"
#include "vp8ratecontrol.h"
#include "vp8quanttable.h"

enum VP8EncStatus {
  VP8ENCSTAT_INIT = 0xA1,
  VP8ENCSTAT_KEYFRAME,
  VP8ENCSTAT_START_FRAME,
  VP8ENCSTAT_ERROR
};

typedef struct {
  int32_t quant[2];
  int32_t zbin[2];
  int32_t round[2];
  int32_t dequant[2];
} qp;

typedef struct {
  /* Approximate bit cost of mode. IOW bits used when selected mode is
   * boolean encoded using appropriate tree and probabilities. Note that
   * this value is scale up with SCALE (=256) */
  int32_t intra16ModeBitCost[4 + 1];
  int32_t intra4ModeBitCost[14 + 1];
} mbs;

typedef struct
{
  uint32_t encStatus;
  uint32_t mbPerFrame;
  uint32_t mbPerRow;
  uint32_t mbPerCol;
  uint32_t frameCnt;
  uint32_t testId;
  uint32_t numRefBuffsLum;
  uint32_t numRefBuffsChr;
  uint32_t prevFrameLost;
  vp8RateControl_s rateControl;
  picBuffer picBuffer;         /* Reference picture container */
  sps sps;                     /* Sequence parameter set */
  ppss ppss;                   /* Picture parameter set */
  vp8buffer buffer[4];         /* Stream buffer per partition */
  qp qpY1[QINDEX_RANGE];  /* Quant table for 1'st order luminance */
  qp qpY2[QINDEX_RANGE];  /* Quant table for 2'nd order luminance */
  qp qpCh[QINDEX_RANGE];  /* Quant table for chrominance */
  mbs mbs;
  asicData_s asic;
  uint32_t* pOutBuf;           /* User given stream output buffer */
  const void* inst;            /* Pointer to this instance for checking */
  entropy entropy[1];
} vp8Instance_s;

#endif
