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

#ifndef __VP8_INIT_H__
#define __VP8_INIT_H__

#include "vp8encapi.h"
#include "vp8instance.h"

bool_e VP8CheckCfg(const VP8EncConfig* pEncCfg);
int32_t VP8GetAllowedWidth(int32_t width, VP8EncPictureType inputType);
VP8EncRet VP8Init(const VP8EncConfig* pEncCfg, vp8Instance_s** instAddr);
void VP8Shutdown(vp8Instance_s* data);
#endif
