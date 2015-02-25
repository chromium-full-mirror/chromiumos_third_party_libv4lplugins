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
#include <stdio.h>

#include "vp8encapi.h"
#include "libvpu/rk_vepu_debug.h"

#include "enccommon.h"
#include "vp8codeframe.h"
#include "vp8init.h"
#include "vp8instance.h"
#include "vp8ratecontrol.h"
#include "vp8putbits.h"

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif

/* Parameter limits */
#define VP8ENC_MAX_PP_INPUT_WIDTH      8176
#define VP8ENC_MAX_PP_INPUT_HEIGHT     8176
#define VP8ENC_MAX_BITRATE             (50000*1200)

/*----------------------------------------------------------------------------
    Function name : VP8EncInit
    Description   : Initialize an encoder instance and
                    returns it to application

    Return type   : VP8EncRet
    Argument      : pEncCfg - initialization parameters
                    instAddr - where to save the created instance
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncInit(const VP8EncConfig* pEncCfg, VP8EncInst* instAddr) {
  VP8EncRet ret;
  vp8Instance_s* pEncInst = NULL;

  VPU_PLG_DBG("VP8EncInit#");

  /* check that right shift on negative numbers is performed signed */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif

  /* Check for illegal inputs */
  if (pEncCfg == NULL || instAddr == NULL) {
    VPU_PLG_ERR("VP8EncInit: ERROR Null argument");
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check that configuration is valid */
  if (VP8CheckCfg(pEncCfg) == ENCHW_NOK) {
    VPU_PLG_ERR("VP8EncInit: ERROR Invalid configuration");
    return VP8ENC_INVALID_ARGUMENT;
  }

  /* Initialize encoder instance and allocate memories */
  ret = VP8Init(pEncCfg, &pEncInst);
  if (ret != VP8ENC_OK) {
    VPU_PLG_ERR("VP8EncInit: ERROR Initialization failed");
    return ret;
  }

  pEncInst->encStatus = VP8ENCSTAT_INIT;

  pEncInst->inst = pEncInst;

  *instAddr = (VP8EncInst)pEncInst;

  VPU_PLG_DBG("VP8EncInit: OK");
  return VP8ENC_OK;
}

/*----------------------------------------------------------------------------

    Function name : VP8EncRelease
    Description   : Releases encoder instance and all associated resource

    Return type   : VP8EncRet
    Argument      : inst - the instance to be released
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncRelease(VP8EncInst inst) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;

  VPU_PLG_DBG("VP8EncRelease#");

  /* Check for illegal inputs */
  if (pEncInst == NULL) {
    VPU_PLG_DBG("VP8EncRelease: ERROR Null argument");
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    VPU_PLG_DBG("VP8EncRelease: ERROR Invalid instance");
    return VP8ENC_INSTANCE_ERROR;
  }

#ifdef TRACE_STREAM
  EncCloseStreamTrace();
#endif

  VP8Shutdown(pEncInst);

  VPU_PLG_DBG("VP8EncRelease: OK");
  return VP8ENC_OK;
}

/*----------------------------------------------------------------------------

    Function name : VP8EncSetCodingCtrl
    Description   : Sets encoding parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pCodeParams - user provided parameters
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncSetCodingCtrl(VP8EncInst inst,
                              const VP8EncCodingCtrl* pCodeParams) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;
  regValues_s* regs;
  sps* sps;
  uint32_t area1 = 0, area2 = 0;

  VPU_PLG_DBG("VP8EncSetCodingCtrl#");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pCodeParams == NULL)) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Null argument\n");
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid instance\n");
    return VP8ENC_INSTANCE_ERROR;
  }

  /* check limits */
  if (pCodeParams->filterLevel > VP8ENC_FILTER_LEVEL_AUTO ||
      pCodeParams->filterSharpness > VP8ENC_FILTER_SHARPNESS_AUTO) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid parameter\n");
    return VP8ENC_INVALID_ARGUMENT;
  }

  if (pCodeParams->cirStart > pEncInst->mbPerFrame ||
      pCodeParams->cirInterval > pEncInst->mbPerFrame) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid CIR value\n");
    return VP8ENC_INVALID_ARGUMENT;
  }

  if (pCodeParams->intraArea.enable) {
    if (!(pCodeParams->intraArea.top <= pCodeParams->intraArea.bottom &&
          pCodeParams->intraArea.bottom < pEncInst->mbPerCol &&
          pCodeParams->intraArea.left <= pCodeParams->intraArea.right &&
          pCodeParams->intraArea.right < pEncInst->mbPerRow)) {
      VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid intraArea\n");
      return VP8ENC_INVALID_ARGUMENT;
    }
  }

  if (pCodeParams->roi1Area.enable) {
    if (!(pCodeParams->roi1Area.top <= pCodeParams->roi1Area.bottom &&
          pCodeParams->roi1Area.bottom < pEncInst->mbPerCol &&
          pCodeParams->roi1Area.left <= pCodeParams->roi1Area.right &&
          pCodeParams->roi1Area.right < pEncInst->mbPerRow)) {
      VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid roi1Area\n");
      return VP8ENC_INVALID_ARGUMENT;
    }
    area1 = (pCodeParams->roi1Area.right + 1 - pCodeParams->roi1Area.left) *
        (pCodeParams->roi1Area.bottom + 1 - pCodeParams->roi1Area.top);
  }

  if (pCodeParams->roi2Area.enable) {
    if (!pCodeParams->roi1Area.enable) {
      VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Roi2 enabled but not Roi1\n");
      return VP8ENC_INVALID_ARGUMENT;
    }
    if (!(pCodeParams->roi2Area.top <= pCodeParams->roi2Area.bottom &&
          pCodeParams->roi2Area.bottom < pEncInst->mbPerCol &&
          pCodeParams->roi2Area.left <= pCodeParams->roi2Area.right &&
          pCodeParams->roi2Area.right < pEncInst->mbPerRow)) {
      VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid roi2Area\n");
      return VP8ENC_INVALID_ARGUMENT;
    }
    area2 = (pCodeParams->roi2Area.right + 1 - pCodeParams->roi2Area.left) *
        (pCodeParams->roi2Area.bottom + 1 - pCodeParams->roi2Area.top);
  }

  if (area1 + area2 >= pEncInst->mbPerFrame) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid roi (whole frame)\n");
    return VP8ENC_INVALID_ARGUMENT;
  }

  if (pCodeParams->roi1DeltaQp < -50 ||
      pCodeParams->roi1DeltaQp > 0 ||
      pCodeParams->roi2DeltaQp < -50 ||
      pCodeParams->roi2DeltaQp > 0) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl: ERROR Invalid ROI delta QP\n");
    return VP8ENC_INVALID_ARGUMENT;
  }

  sps = &pEncInst->sps;

  /* TODO check limits */
  sps->filterType        = pCodeParams->filterType;
  if (pCodeParams->filterLevel == VP8ENC_FILTER_LEVEL_AUTO) {
    sps->autoFilterLevel = 1;
    sps->filterLevel     = 0;
  } else {
    sps->autoFilterLevel = 0;
    sps->filterLevel     = pCodeParams->filterLevel;
  }

  if (pCodeParams->filterSharpness == VP8ENC_FILTER_SHARPNESS_AUTO) {
    sps->autoFilterSharpness = 1;
    sps->filterSharpness     = 0;
  } else {
    sps->autoFilterSharpness = 0;
    sps->filterSharpness     = pCodeParams->filterSharpness;
  }

  sps->dctPartitions     = pCodeParams->dctPartitions;
  sps->partitionCnt      = 2 + (1 << sps->dctPartitions);
  sps->refreshEntropy    = pCodeParams->errorResilient ? 0 : 1;
  sps->quarterPixelMv    = pCodeParams->quarterPixelMv;
  sps->splitMv           = pCodeParams->splitMv;

  regs = &pEncInst->asic.regs;
  regs->cirStart = pCodeParams->cirStart;
  regs->cirInterval = pCodeParams->cirInterval;
  if (pCodeParams->intraArea.enable) {
    regs->intraAreaTop = pCodeParams->intraArea.top;
    regs->intraAreaLeft = pCodeParams->intraArea.left;
    regs->intraAreaBottom = pCodeParams->intraArea.bottom;
    regs->intraAreaRight = pCodeParams->intraArea.right;
  }
  if (pCodeParams->roi1Area.enable) {
    regs->roi1Top = pCodeParams->roi1Area.top;
    regs->roi1Left = pCodeParams->roi1Area.left;
    regs->roi1Bottom = pCodeParams->roi1Area.bottom;
    regs->roi1Right = pCodeParams->roi1Area.right;
  }
  if (pCodeParams->roi2Area.enable) {
    regs->roi2Top = pCodeParams->roi2Area.top;
    regs->roi2Left = pCodeParams->roi2Area.left;
    regs->roi2Bottom = pCodeParams->roi2Area.bottom;
    regs->roi2Right = pCodeParams->roi2Area.right;
  }

  /* ROI setting updates the segmentation map usage */
  if (pCodeParams->roi1Area.enable || pCodeParams->roi2Area.enable)
    pEncInst->ppss.pps->segmentEnabled = 1;
  else {
    pEncInst->ppss.pps->segmentEnabled = 0;
    /* Disabling ROI will clear the segment ID map */
    memset(pEncInst->asic.segmentMap.vir_addr, 0,
           pEncInst->asic.segmentMap.size);
  }
  pEncInst->ppss.pps->sgm.mapModified = true;

  regs->roi1DeltaQp = -pCodeParams->roi1DeltaQp;
  regs->roi2DeltaQp = -pCodeParams->roi2DeltaQp;

  VPU_PLG_DBG("VP8EncSetCodingCtrl: OK");
  return VP8ENC_OK;
}

/*----------------------------------------------------------------------------

    Function name : VP8EncGetCodingCtrl
    Description   : Returns current encoding parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pCodeParams - palce where parameters are returned
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncGetCodingCtrl(VP8EncInst inst,
                              VP8EncCodingCtrl* pCodeParams) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;
  regValues_s* regs;
  sps* sps;

  VPU_PLG_DBG("VP8EncGetCodingCtrl#");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pCodeParams == NULL)) {
    VPU_PLG_ERR("VP8EncGetCodingCtrl: ERROR Null argument\n");
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    VPU_PLG_ERR("VP8EncGetCodingCtrl: ERROR Invalid instance\n");
    return VP8ENC_INSTANCE_ERROR;
  }

  sps = &pEncInst->sps;

  pCodeParams->interpolationFilter    = 1;    /* Only bicubic supported */
  pCodeParams->dctPartitions          = sps->dctPartitions;
  pCodeParams->quarterPixelMv         = sps->quarterPixelMv;
  pCodeParams->splitMv                = sps->splitMv;
  pCodeParams->filterType             = sps->filterType;
  pCodeParams->errorResilient         = !sps->refreshEntropy;

  if (sps->autoFilterLevel)
    pCodeParams->filterLevel            = VP8ENC_FILTER_LEVEL_AUTO;
  else
    pCodeParams->filterLevel            = sps->filterLevel;

  if (sps->autoFilterSharpness)
    pCodeParams->filterSharpness        = VP8ENC_FILTER_SHARPNESS_AUTO;
  else
    pCodeParams->filterSharpness        = sps->filterSharpness;

  regs = &pEncInst->asic.regs;
  pCodeParams->cirStart = regs->cirStart;
  pCodeParams->cirInterval = regs->cirInterval;
  pCodeParams->intraArea.enable =
    regs->intraAreaTop < pEncInst->mbPerCol ? 1 : 0;
  pCodeParams->intraArea.top    = regs->intraAreaTop;
  pCodeParams->intraArea.left   = regs->intraAreaLeft;
  pCodeParams->intraArea.bottom = regs->intraAreaBottom;
  pCodeParams->intraArea.right  = regs->intraAreaRight;

  VPU_PLG_DBG("VP8EncGetCodingCtrl: OK\n");
  return VP8ENC_OK;
}

/*----------------------------------------------------------------------------

    Function name : VP8EncSetRateCtrl
    Description   : Sets rate control parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pRateCtrl - user provided parameters
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncSetRateCtrl(VP8EncInst inst,
                            const VP8EncRateCtrl* pRateCtrl) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;
  vp8RateControl_s rc_tmp;

  VPU_PLG_DBG("VP8EncSetRateCtrl#");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pRateCtrl == NULL)) {
    VPU_PLG_ERR("VP8EncSetRateCtrl: ERROR Null argument\n");
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    VPU_PLG_ERR("VP8EncSetRateCtrl: ERROR Invalid instance\n");
    return VP8ENC_INSTANCE_ERROR;
  }

  /* TODO Check for invalid values */

  rc_tmp = pEncInst->rateControl;
  rc_tmp.qpHdr                    = pRateCtrl->qpHdr;
  rc_tmp.picRc                    = pRateCtrl->pictureRc;
  rc_tmp.picSkip                  = pRateCtrl->pictureSkip;
  rc_tmp.qpMin                    = pRateCtrl->qpMin;
  rc_tmp.qpMax                    = pRateCtrl->qpMax;
  rc_tmp.virtualBuffer.bitRate    = pRateCtrl->bitPerSecond;
  rc_tmp.gopLen                   = pRateCtrl->bitrateWindow;
  rc_tmp.intraQpDelta             = pRateCtrl->intraQpDelta;
  rc_tmp.fixedIntraQp             = pRateCtrl->fixedIntraQp;
  rc_tmp.intraPictureRate         = pRateCtrl->intraPictureRate;
  rc_tmp.goldenPictureRate        = pRateCtrl->goldenPictureRate;
  rc_tmp.altrefPictureRate        = pRateCtrl->altrefPictureRate;
  rc_tmp.outRateNum               = pRateCtrl->outRateNum;
  rc_tmp.outRateDenom             = pRateCtrl->outRateDenom;

  pEncInst->encStatus = VP8ENCSTAT_INIT;

  VP8InitRc(&rc_tmp, true);

  /* Set final values into instance */
  pEncInst->rateControl = rc_tmp;

  VPU_PLG_DBG("VP8EncSetRateCtrl: OK\n");
  return VP8ENC_OK;
}

/*----------------------------------------------------------------------------

    Function name : VP8EncGetRateCtrl
    Description   : Return current rate control parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pRateCtrl - place where parameters are returned
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncGetRateCtrl(VP8EncInst inst, VP8EncRateCtrl* pRateCtrl) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;

  VPU_PLG_DBG("VP8EncGetRateCtrl#");

  /* Check for illegal inputs */
  if (pEncInst == NULL || pRateCtrl == NULL) {
    VPU_PLG_ERR("%s ERROR, Instance doesn't initialized\n", __func__);
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    VPU_PLG_ERR("VP8EncGetRateCtrl: ERROR Invalid instance\n");
    return VP8ENC_INSTANCE_ERROR;
  }

  pRateCtrl->qpHdr            = pEncInst->rateControl.qpHdr;
  pRateCtrl->pictureRc        = pEncInst->rateControl.picRc;
  pRateCtrl->pictureSkip      = pEncInst->rateControl.picSkip;
  pRateCtrl->qpMin            = pEncInst->rateControl.qpMin;
  pRateCtrl->qpMax            = pEncInst->rateControl.qpMax;
  pRateCtrl->bitPerSecond     = pEncInst->rateControl.virtualBuffer.bitRate;
  pRateCtrl->bitrateWindow    = pEncInst->rateControl.gopLen;
  pRateCtrl->intraQpDelta     = pEncInst->rateControl.intraQpDelta;
  pRateCtrl->fixedIntraQp     = pEncInst->rateControl.fixedIntraQp;
  pRateCtrl->intraPictureRate = pEncInst->rateControl.intraPictureRate;
  pRateCtrl->goldenPictureRate = pEncInst->rateControl.goldenPictureRate;
  pRateCtrl->altrefPictureRate = pEncInst->rateControl.altrefPictureRate;
  pRateCtrl->outRateNum       = pEncInst->rateControl.outRateNum;
  pRateCtrl->outRateDenom     = pEncInst->rateControl.outRateDenom;

  VPU_PLG_DBG("VP8EncGetRateCtrl: OK\n");
  return VP8ENC_OK;
}

VP8EncRet VP8EncStrmEncodeResult(VP8EncInst inst, VP8EncOut* pEncOut,
                                 uint32_t outputStreamSize) {

  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;
  picBuffer* picBuffer;

  pEncOut->frameSize = outputStreamSize;

  picBuffer = &pEncInst->picBuffer;

  /* Rate control action after frame */
  VP8AfterPicRc(&pEncInst->rateControl, outputStreamSize);

  if (picBuffer->cur_pic->i_frame) {
    pEncOut->codingType = VP8ENC_INTRA_FRAME;
    pEncOut->arf = pEncOut->grf = pEncOut->ipf = 0;
  } else {
    pEncOut->codingType = VP8ENC_PREDICTED_FRAME;
    pEncOut->ipf = picBuffer->refPicList[0].search ? VP8ENC_REFERENCE : 0;
    pEncOut->grf = picBuffer->refPicList[1].search ? VP8ENC_REFERENCE : 0;
    pEncOut->arf = picBuffer->refPicList[2].search ? VP8ENC_REFERENCE : 0;
  }

  /* Mark which reference frame was refreshed */
  pEncOut->arf |= picBuffer->cur_pic->arf ? VP8ENC_REFRESH : 0;
  pEncOut->grf |= picBuffer->cur_pic->grf ? VP8ENC_REFRESH : 0;
  pEncOut->ipf |= picBuffer->cur_pic->ipf ? VP8ENC_REFRESH : 0;

  UpdatePictureBuffer(picBuffer);

  /* Frame was encoded so increment frame number */
  pEncInst->frameCnt++;
  pEncInst->encStatus = VP8ENCSTAT_START_FRAME;
  pEncInst->prevFrameLost = 0;

  VPU_PLG_DBG("VP8EncStrmEncode: OK\n");
  return 0;
}

void VP8EncGetFrameHeader(VP8EncInst inst, uint8_t** frmhdr, uint32_t* size) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;

  *frmhdr = pEncInst->asic.frmhdr;
  *size = pEncInst->asic.frmHdrBufLen;
}

void VP8EncGetCabacCtx(VP8EncInst inst, uint8_t** cabac, uint32_t* size) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;

  *cabac = (uint8_t*)pEncInst->asic.cabacCtx.vir_addr;
  *size = pEncInst->asic.cabacCtx.size;
}

void VP8EncGetSegmentMap(VP8EncInst inst, uint8_t** segmap, uint32_t* size) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;

  *segmap = (uint8_t*)pEncInst->asic.segmentMap.vir_addr;
  *size = pEncInst->asic.segmentMap.size;
}

void VP8EncGetRegs(VP8EncInst inst, uint32_t** regs, uint32_t* size) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;

  *regs = pEncInst->asic.regs.regMirror;
  *size = sizeof(pEncInst->asic.regs.regMirror);
}

VP8EncRet VP8EncSetProbCnt(VP8EncInst inst, uint8_t* probcnt, uint32_t size) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;
  if (probcnt == NULL || size > pEncInst->asic.probCount.size) {
    VPU_PLG_ERR("Invalid input parameter\n");
    return -1;
  }

  memcpy(pEncInst->asic.probCount.vir_addr, probcnt, size);

  return 0;
}

/*----------------------------------------------------------------------------

    Function name : VP8EncStrmEncode
    Description   : Encodes a new picture
    Return type   : VP8EncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
-----------------------------------------------------------------------------*/
VP8EncRet VP8EncStrmEncode(VP8EncInst inst, const VP8EncIn* pEncIn,
                           VP8EncOut* pEncOut, EncoderParameters* cml) {
  vp8Instance_s* pEncInst = (vp8Instance_s*)inst;
  picBuffer* picBuffer;
  int32_t i;
  VP8EncPictureCodingType ct;

  VPU_PLG_DBG("VP8EncStrmEncode#\n");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pEncIn == NULL) || (pEncOut == NULL)) {
    VPU_PLG_ERR("VP8EncStrmEncode: ERROR Null argument\n");
    return VP8ENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    VPU_PLG_ERR("VP8EncStrmEncode: ERROR Invalid instance\n");
    return VP8ENC_INSTANCE_ERROR;
  }

  /* Clear the output structure */
  pEncOut->codingType = VP8ENC_NOTCODED_FRAME;
  pEncOut->frameSize = 0;
  for (i = 0; i < 9; i++) {
    pEncOut->pOutBuf[i] = NULL;
    pEncOut->streamSize[i] = 0;
  }

  /* Check status, ERROR not allowed */
  if ((pEncInst->encStatus != VP8ENCSTAT_INIT) &&
      (pEncInst->encStatus != VP8ENCSTAT_KEYFRAME) &&
      (pEncInst->encStatus != VP8ENCSTAT_START_FRAME)) {
    VPU_PLG_ERR("VP8EncStrmEncode: ERROR Invalid status\n");
    return VP8ENC_INVALID_STATUS;
  }

  /* Choose frame coding type */
  ct = pEncIn->codingType;

  /* Status may affect the frame coding type */
  if ((pEncInst->encStatus == VP8ENCSTAT_INIT) ||
      (pEncInst->encStatus == VP8ENCSTAT_KEYFRAME))
    ct = VP8ENC_INTRA_FRAME;

  /* Divide stream buffer for every partition */
  {
    uint8_t* pStart = (uint8_t*)pEncInst->asic.frmhdr;
    uint32_t bufSize = pEncInst->asic.frmHdrBufLen;
    uint8_t* pEnd;
    int32_t status = ENCHW_OK;

    /* Frame tag 10 bytes (I-frame) or 3 bytes (P-frame),
     * written by SW at end of frame */
    pEnd = pStart + 3;
    if (ct == VP8ENC_INTRA_FRAME) pEnd += 7;
    if (VP8SetBuffer(&pEncInst->buffer[0], pStart, pEnd - pStart) == ENCHW_NOK)
      status = ENCHW_NOK;

    pStart = pEnd;
    pEnd = pStart + bufSize;
    if (VP8SetBuffer(&pEncInst->buffer[1], pStart, pEnd - pStart) == ENCHW_NOK)
      status = ENCHW_NOK;

    if (status == ENCHW_NOK) {
      VPU_PLG_ERR("VP8 Set frame header buffer failed\n");
      return status;
    }
  }

  /* Initialize picture buffer and ref pic list according to frame type */
  picBuffer = &pEncInst->picBuffer;
  picBuffer->cur_pic->show    = 1;
  picBuffer->cur_pic->poc     = pEncInst->frameCnt;
  picBuffer->cur_pic->i_frame = (ct == VP8ENC_INTRA_FRAME);
  InitializePictureBuffer(picBuffer);

  /* Set picture buffer according to frame coding type */
  if (ct == VP8ENC_PREDICTED_FRAME) {
    picBuffer->cur_pic->p_frame = 1;
    picBuffer->cur_pic->arf = (pEncIn->arf & VP8ENC_REFRESH) ? 1 : 0;
    picBuffer->cur_pic->grf = (pEncIn->grf & VP8ENC_REFRESH) ? 1 : 0;
    picBuffer->cur_pic->ipf = (pEncIn->ipf & VP8ENC_REFRESH) ? 1 : 0;
    picBuffer->refPicList[0].search = (pEncIn->ipf & VP8ENC_REFERENCE) ? 1 : 0;
    picBuffer->refPicList[1].search = (pEncIn->grf & VP8ENC_REFERENCE) ? 1 : 0;
    picBuffer->refPicList[2].search = (pEncIn->arf & VP8ENC_REFERENCE) ? 1 : 0;
  }

  /* Rate control */
  VP8BeforePicRc(&pEncInst->rateControl, pEncIn->timeIncrement,
                 picBuffer->cur_pic->i_frame);

  /* Rate control may choose to skip the frame */
  if (pEncInst->rateControl.frameCoded == ENCHW_NO) {
    VPU_PLG_DBG("VP8EncStrmEncode: OK, frame skipped");
    return VP8ENC_FRAME_READY;
  }

  /* TODO: RC can set frame to grf and copy grf to arf */
  if (pEncInst->rateControl.goldenPictureRate) {
    picBuffer->cur_pic->grf = 1;
    if (!picBuffer->cur_pic->arf)
      picBuffer->refPicList[1].arf = 1;
  }

  /* Set some frame coding parameters before internal test configure */
  VP8SetFrameParams(pEncInst);

#ifdef TRACE_STREAM
  traceStream.frameNum = pEncInst->frameCnt;
  traceStream.id = 0; /* Stream generated by SW */
  traceStream.bitCnt = 0;  /* New frame */
#endif

  /* Get the reference frame buffers from picture buffer */
  PictureBufferSetRef(picBuffer, &pEncInst->asic);

  /* Code one frame */
  VP8CodeFrame(pEncInst, cml);

  return VP8ENC_OK;
}

