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

#include <malloc.h>
#include <memory.h>
#include "encasiccontroller.h"
#include "enccommon.h"

int32_t VP8_EncAsicMemAlloc_V2(asicData_s* asic, uint32_t width, uint32_t height,
                               uint32_t encodingType, uint32_t numRefBuffsLum,
                               uint32_t numRefBuffsChr) {
  uint32_t mbTotal, cabacTablSize;
  regValues_s* regs;

  ASSERT(asic != NULL);
  ASSERT(width != 0);
  ASSERT(height != 0);
  ASSERT((width % 4) == 0);

  regs = &asic->regs;

  regs->codingType = encodingType;

  width = (width + 15) / 16;
  height = (height + 15) / 16;

  mbTotal = width * height;

  /* H264: CABAC context tables: all qps, intra+inter, 464 bytes/table.
   * VP8: The same table is used for probability tables, 1208 bytes. */
  cabacTablSize = 8 * 55 + 8 * 96;

  if (VPUMallocLinear(&asic->cabacCtx, cabacTablSize) != 0) {
    VP8_EncAsicMemFree_V2(asic);
    return ENCHW_NOK;
  }
  regs->cabacCtxBase = asic->cabacCtx.phy_addr;

  /* VP8: Table of counter for probability updates. */
  if (VPUMallocLinear(&asic->probCount, ASIC_VP8_PROB_COUNT_SIZE) != 0) {
    VP8_EncAsicMemFree_V2(asic);
    return ENCHW_NOK;
  }
  regs->probCountBase = asic->probCount.phy_addr;

  /* VP8: Segmentation map, 4 bits/mb, 64-bit multiple. */
  if (VPUMallocLinear(&asic->segmentMap, (mbTotal * 4 + 63) / 64 * 8) != 0) {
    VP8_EncAsicMemFree_V2(asic);
    return ENCHW_NOK;
  }
  regs->segmentMapBase = asic->segmentMap.phy_addr;

  memset(asic->segmentMap.vir_addr, 0, asic->segmentMap.size);

  asic->frmhdr = (uint8_t*)(((int)asic->hdr + 7) & (~7));
  asic->frmHdrBufLen = FRAME_HEADER_SIZE;

  return ENCHW_OK;
}

void VP8_EncAsicMemFree_V2(asicData_s* asic) {
  ASSERT(asic != NULL);
  VPUFreeLinear(&asic->cabacCtx);
  VPUFreeLinear(&asic->probCount);
}

