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

#include "vp8picparameterset.h"

#include <malloc.h>
#include <memory.h>

#include "enccommon.h"
#include "libvpu/rk_vepu_debug.h"

int32_t PicParameterSetAlloc(ppss* ppss) {
  ppss->size = 1;
  ppss->store = (pps*) malloc(ppss->size * sizeof(pps));
  if (ppss->store == NULL) {
    VPU_PLG_ERR("Fail to malloc ppss store.\n");
    return ENCHW_NOK;
  }
  return ENCHW_OK;
}

void PicParameterSetFree(ppss* ppss) {
  free(ppss->store);
  ppss->store = NULL;
}
