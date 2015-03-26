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

#include <memory.h>
#include <malloc.h>
#include "vp8init.h"
#include "vp8macroblocktools.h"
#include "enccommon.h"

#define VP8ENC_MIN_ENC_WIDTH       132     /* 144 - 12 pixels overfill */
#define VP8ENC_MAX_ENC_WIDTH       4080
#define VP8ENC_MIN_ENC_HEIGHT      96
#define VP8ENC_MAX_ENC_HEIGHT      4080

#define VP8ENC_MAX_MBS_PER_PIC     65025   /* 4080x4080 */

static void SetParameter(vp8Instance_s* inst, const VP8EncConfig* pEncCfg);

static int32_t SetPictureBuffer(vp8Instance_s* inst);

/*------------------------------------------------------------------------------

    VP8CheckCfg

    Function checks that the configuration is valid.

    Input   pEncCfg Pointer to configuration structure.

    Return  ENCHW_OK      The configuration is valid.
            ENCHW_NOK     Some of the parameters in configuration are not valid.

------------------------------------------------------------------------------*/
bool_e VP8CheckCfg(const VP8EncConfig* pEncCfg) {
  ASSERT(pEncCfg);

  /* Encoded image width limits, multiple of 4 */
  if (pEncCfg->width < VP8ENC_MIN_ENC_WIDTH ||
      pEncCfg->width > VP8ENC_MAX_ENC_WIDTH || (pEncCfg->width & 0x3) != 0)
    return ENCHW_NOK;

  /* Encoded image height limits */
  if (pEncCfg->height < VP8ENC_MIN_ENC_HEIGHT ||
      pEncCfg->height > VP8ENC_MAX_ENC_HEIGHT)
    return ENCHW_NOK;

  /* total macroblocks per picture limit */
  if (((pEncCfg->height + 15) / 16) * ((pEncCfg->width + 15) / 16) >
          VP8ENC_MAX_MBS_PER_PIC) {
    return ENCHW_NOK;
  }

  /* Check frame rate */
  if (pEncCfg->frameRateNum < 1 || pEncCfg->frameRateNum > ((1 << 20) - 1))
    return ENCHW_NOK;

  if (pEncCfg->frameRateDenom < 1) {
    return ENCHW_NOK;
  }

  /* special allowal of 1000/1001, 0.99 fps by customer request */
  if (pEncCfg->frameRateDenom > pEncCfg->frameRateNum &&
      !(pEncCfg->frameRateDenom == 1001 && pEncCfg->frameRateNum == 1000)) {
    return ENCHW_NOK;
  }

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    VP8Init

    Function initializes the Encoder and creates new encoder instance.

    Input   pEncCfg     Encoder configuration.
            instAddr    Pointer to instance will be stored in this address

    Return  VP8ENC_OK
            VP8ENC_MEMORY_ERROR
            VP8ENC_EWL_ERROR
            VP8ENC_EWL_MEMORY_ERROR
            VP8ENC_INVALID_ARGUMENT

------------------------------------------------------------------------------*/
VP8EncRet VP8Init(const VP8EncConfig* pEncCfg, vp8Instance_s** instAddr) {
  vp8Instance_s* inst = NULL;
  VP8EncRet ret = VP8ENC_OK;
  int32_t i;

  ASSERT(pEncCfg);
  ASSERT(instAddr);

  *instAddr = NULL;

  /* Encoder instance */
  inst = (vp8Instance_s*)malloc(sizeof(vp8Instance_s));

  if (inst == NULL) {
    ret = VP8ENC_MEMORY_ERROR;
    goto err;
  }

  memset(inst, 0, sizeof(vp8Instance_s));

  /* Set parameters depending on user config */
  SetParameter(inst, pEncCfg);
  InitQuantTables(inst);

  if (SetPictureBuffer(inst) != ENCHW_OK) {
    ret = VP8ENC_INVALID_ARGUMENT;
    goto err;
  }

  VP8InitRc(&inst->rateControl, 1);

  /* Initialize ASIC */
  (void) VP8_EncAsicControllerInit(&inst->asic);

  /* Allocate internal SW/HW shared memories */
  if (VP8_EncAsicMemAlloc_V2(&inst->asic,
                             pEncCfg->width,
                             pEncCfg->height,
                             ASIC_VP8, inst->numRefBuffsLum,
                             inst->numRefBuffsChr) != ENCHW_OK) {
    ret = VP8ENC_EWL_MEMORY_ERROR;
    goto err;
  }

  /* Assign allocated HW frame buffers into picture buffer */
  inst->picBuffer.size = inst->numRefBuffsLum;
  for (i = 0; i < inst->numRefBuffsLum; i++)
    inst->picBuffer.refPic[i].picture.lum = i;
  for (i = 0; i < inst->numRefBuffsChr; i++)
    inst->picBuffer.refPic[i].picture.cb = i;

  *instAddr = inst;

  inst->asic.regs.intra16Favor    = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.prevModeFavor   = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.interFavor      = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.skipPenalty     = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.diffMvPenalty[0] = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.diffMvPenalty[1] = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.diffMvPenalty[2] = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.splitPenalty[0] = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.splitPenalty[1] = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.splitPenalty[2] = 0x3FF; /* No 8x4 MVs in VP8 */
  inst->asic.regs.splitPenalty[3] = ASIC_PENALTY_UNDEFINED;
  inst->asic.regs.zeroMvFavorDiv2 = 0; /* No favor for VP8 */

  /* Disable intra and ROI areas by default */
  inst->asic.regs.intraAreaTop = inst->asic.regs.intraAreaBottom =
      inst->asic.regs.intraAreaLeft = inst->asic.regs.intraAreaRight =
      inst->asic.regs.roi1Top = inst->asic.regs.roi1Bottom =
      inst->asic.regs.roi1Left = inst->asic.regs.roi1Right =
      inst->asic.regs.roi2Top = inst->asic.regs.roi2Bottom =
      inst->asic.regs.roi2Left = inst->asic.regs.roi2Right = 255;

  return ret;

 err:
  free(inst);
  return ret;
}

/*------------------------------------------------------------------------------

    VP8Shutdown

    Function frees the encoder instance.

    Input   vp8Instance_s *    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
void VP8Shutdown(vp8Instance_s* data) {
  ASSERT(data);

  VP8_EncAsicMemFree_V2(&data->asic);

  PictureBufferFree(&data->picBuffer);

  PicParameterSetFree(&data->ppss);

  free(data);
}

/*------------------------------------------------------------------------------

    SetParameter

    Set all parameters in instance to valid values depending on user config.

------------------------------------------------------------------------------*/
void SetParameter(vp8Instance_s* inst, const VP8EncConfig* pEncCfg) {
  int32_t width, height;
  sps* sps = &inst->sps;

  ASSERT(inst);

  /* Internal images, next macroblock boundary */
  width = 16 * ((pEncCfg->width + 15) / 16);
  height = 16 * ((pEncCfg->height + 15) / 16);

  /* Luma ref buffers can be read and written at the same time,
   * but chroma buffers must be one for reading and one for writing */
  inst->numRefBuffsLum    = pEncCfg->refFrameAmount;
  inst->numRefBuffsChr    = inst->numRefBuffsLum + 1;

  /* Macroblock */
  inst->mbPerFrame        = width / 16 * height / 16;
  inst->mbPerRow          = width / 16;
  inst->mbPerCol          = height / 16;

  /* Sequence parameter set */
  sps->picWidthInPixel    = pEncCfg->width;
  sps->picHeightInPixel   = pEncCfg->height;
  sps->picWidthInMbs      = width / 16;
  sps->picHeightInMbs     = height / 16;

  sps->horizontalScaling = 0; /* TODO, not supported yet */
  sps->verticalScaling   = 0; /* TODO, not supported yet */
  sps->colorType         = 0; /* TODO, not supported yet */
  sps->clampType         = 0; /* TODO, not supported yet */
  sps->dctPartitions     = 0; /* Dct data partitions 0=1, 1=2, 2=4, 3=8 */
  sps->partitionCnt      = 2 + (1 << sps->dctPartitions);
  sps->profile           = 1; /* Currently ASIC only supports bilinear ipol */
  sps->filterType        = 0;
  sps->filterLevel       = 0;
  sps->filterSharpness   = 0;
  sps->autoFilterLevel     = 1; /* Automatic filter values by default. */
  sps->autoFilterSharpness = 1;
  sps->quarterPixelMv    = 1; /* 1=adaptive by default */
  sps->splitMv           = 1; /* 1=adaptive by default */
  sps->refreshEntropy    = 1; /* 0=default probs, 1=prev frame probs */
  memset(sps->singBias, 0, sizeof(sps->singBias));

  sps->filterDeltaEnable = true;
  memset(sps->refDelta, 0, sizeof(sps->refDelta));
  memset(sps->modeDelta, 0, sizeof(sps->modeDelta));

  /* Rate control */
  inst->rateControl.virtualBuffer.bitRate = 1000000;
  inst->rateControl.qpHdr         = -1;
  inst->rateControl.picRc         = ENCHW_YES;
  inst->rateControl.picSkip       = ENCHW_NO;
  inst->rateControl.qpMin         = 0;
  inst->rateControl.qpMax         = 127;
  inst->rateControl.gopLen        = 150;
  inst->rateControl.mbPerPic      = inst->mbPerFrame;
  inst->rateControl.outRateDenom  = pEncCfg->frameRateDenom;
  inst->rateControl.outRateNum    = pEncCfg->frameRateNum;
}

int32_t SetPictureBuffer(vp8Instance_s* inst) {
  picBuffer* picBuffer = &inst->picBuffer;
  sps* sps = &inst->sps;
  int32_t width, height;

  width = sps->picWidthInMbs * 16;
  height = sps->picHeightInMbs * 16;
  PictureBufferAlloc(picBuffer, width, height);

  width = sps->picWidthInMbs;
  height = sps->picHeightInMbs;
  if (PicParameterSetAlloc(&inst->ppss) != ENCHW_OK)
    return ENCHW_NOK;

  inst->ppss.pps = inst->ppss.store;
  inst->ppss.pps->segmentEnabled  = 0; /* Segmentation disabled by default. */
  inst->ppss.pps->sgm.mapModified = 0;

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    Round the width to the next multiple of 8 or 16 depending on YUV type.

------------------------------------------------------------------------------*/
int32_t VP8GetAllowedWidth(int32_t width, VP8EncPictureType inputType) {
  if (inputType == VP8ENC_YUV420_PLANAR) {
    /* Width must be multiple of 16 to make
     * chrominance row 64-bit aligned */
    return ((width + 15) / 16) * 16;
  } else {   /* VP8ENC_YUV420_SEMIPLANAR */
    /* VP8ENC_YUV422_INTERLEAVED_YUYV */
    /* VP8ENC_YUV422_INTERLEAVED_UYVY */
    return ((width + 7) / 8) * 8;
  }
}
