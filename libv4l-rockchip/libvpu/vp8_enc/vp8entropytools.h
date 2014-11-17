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

#ifndef _VP8ENTROPY_TOOLS_H_
#define _VP8ENTROPY_TOOLS_H_

#include <stdint.h>

typedef struct {
  int32_t skipFalseProb;
  int32_t intraProb;
  int32_t lastProb;
  int32_t gfProb;
  int32_t kfYmodeProb[4];
  int32_t YmodeProb[4];
  int32_t kfUVmodeProb[3];
  int32_t UVmodeProb[3];
  int32_t kfBmodeProb[10][10][9];
  int32_t BmodeProb[9];
  int32_t coeffProb[4][8][3][11];
  int32_t oldCoeffProb[4][8][3][11];
  int32_t mvRefProb[4];
  int32_t mvProb[2][19];
  int32_t oldMvProb[2][19];
  int32_t subMvPartProb[3];   /* TODO use pointer directly to subMvPartProb */
  int32_t subMvRefProb[5][3]; /* TODO use pointer directly to subMvRefProb */
  int32_t defaultCoeffProbFlag;   /* Flag for coeffProb == defaultCoeffProb */
  int32_t updateCoeffProbFlag;    /* Flag for coeffProb != oldCoeffProb */
  int32_t segmentProb[3];
} entropy;

#endif
