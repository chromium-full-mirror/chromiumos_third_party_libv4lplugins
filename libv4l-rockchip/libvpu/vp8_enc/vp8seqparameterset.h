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

#ifndef _VP8SEQ_PARAMETER_SET_H_
#define _VP8SEQ_PARAMETER_SET_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int32_t picWidthInMbs;
  int32_t picHeightInMbs;
  int32_t picWidthInPixel;
  int32_t picHeightInPixel;
  int32_t horizontalScaling;
  int32_t verticalScaling;
  int32_t colorType;
  int32_t clampType;
  int32_t dctPartitions;  /* Dct data partitions 0=1, 1=2, 2=4, 3=8 */
  int32_t partitionCnt;   /* Abbreviation:  2+(1<<prm->dctPartitions) */
  int32_t profile;
  int32_t filterType;
  int32_t filterLevel;
  int32_t filterSharpness;
  int32_t quarterPixelMv;
  int32_t splitMv;
  int32_t singBias[3];    /* SingBias: 0 = ipf, 1 = grf, 2 = arf */

  int32_t autoFilterLevel;
  int32_t autoFilterSharpness;
  bool filterDeltaEnable;
  int32_t modeDelta[4];
  int32_t oldModeDelta[4];
  int32_t refDelta[4];
  int32_t oldRefDelta[4];

  int32_t refreshEntropy;
} sps;

#endif
