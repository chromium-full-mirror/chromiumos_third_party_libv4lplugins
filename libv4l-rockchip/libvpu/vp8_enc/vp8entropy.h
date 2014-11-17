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

#ifndef VP8ENTROPY_H
#define VP8ENTROPY_H

#include "vp8entropytools.h"
#include "vp8putbits.h"
#include "vp8instance.h"

void EncSwapEndianess(uint32_t* buf, uint32_t sizeBytes);
void InitEntropy(vp8Instance_s* inst);
void WriteEntropyTables(vp8Instance_s* inst);
void CoeffProb(vp8buffer* buffer, int32_t curr[4][8][3][11],
               int32_t prev[4][8][3][11]);
void MvProb(vp8buffer* buffer, int32_t curr[2][19], int32_t prev[2][19]);
int32_t CostMv(int32_t mvd, int32_t* mvProb);

#endif
