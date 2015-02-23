/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "rk_vp8encapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enccommon.h"
#include "vp8encapi.h"
#include "vp8instance.h"
#include "libvpu/rk_vepu_debug.h"
#include "libvpu/vp8_enc/vpu_mem.h"

/* Value for parameter to use API default */
#define DEFAULT -100

/* Intermediate Video File Format */
#define IVF_HDR_BYTES       32
#define IVF_FRM_BYTES       12

#define PRIVATE_DATA_HEADER_SIZE  192

static int rk_vp8_encoder_after_encode(struct rk_vp8_encoder *enc,
                                      uint32_t outputStreamSize);
static int rk_vp8_encoder_before_encode(struct rk_vp8_encoder *enc);
static int rk_vp8_encoder_init(struct rk_vp8_encoder *enc,
                               struct rk_vepu_init_param *enc_parms);
static void rk_vp8_encoder_genpriv(struct rk_vp8_encoder *enc);
static void rk_vp8_encoder_getpriv(struct rk_vp8_encoder *enc,
                                   uint8_t **priv_data, uint32_t *size);
static void rk_vp8_encoder_setconfig(struct rk_vp8_encoder *enc,
                                     struct rk_vepu_runtime_param *param);
static void rk_vp8_encoder_store_priv_data(struct rk_vp8_encoder *enc,
                                          uint8_t *priv, uint32_t size,
                                          int32_t type);
static int rk_vp8_encoder_updatepriv(struct rk_vp8_encoder *enc,
                                     void *config, uint32_t cfglen);
static int OpenEncoder(EncoderParameters* cml, VP8EncInst* pEnc);
static void CloseEncoder(VP8EncInst encoder);
static void MaAddFrame(ma_s* ma, int32_t frameSizeBits);
static int32_t Ma(ma_s* ma);
static void SetDefaultParameter(EncoderParameters* tb);


static int rk_vp8_encoder_init(struct rk_vp8_encoder *enc,
                              struct rk_vepu_init_param *enc_parms) {
  EncoderParameters* cml = &enc->cmdl;

  SetDefaultParameter(cml);

  /* modify parameters using input encode setting */
  cml->lumWidthSrc = enc_parms->width;
  cml->lumHeightSrc = enc_parms->height;
  cml->width = enc_parms->width;
  cml->height = enc_parms->height;

  switch (enc_parms->input_format) {
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      cml->inputFormat = VP8ENC_YUV420_PLANAR;
      break;
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      cml->inputFormat = VP8ENC_YUV420_SEMIPLANAR;
      break;
    case V4L2_PIX_FMT_YUYV:
      cml->inputFormat = VP8ENC_YUV422_INTERLEAVED_YUYV;
      break;
    case V4L2_PIX_FMT_UYVY:
      cml->inputFormat = VP8ENC_YUV422_INTERLEAVED_UYVY;
      break;
    default:
      VPU_PLG_ERR("Unsupport format 0x%08x\n", enc_parms->input_format);
      return -1;
  }

  /* Encoder initialization */
  if (OpenEncoder(cml, &enc->encoder) != 0)
    return -1;

  /* First frame is always intra with zero time increment */
  enc->encIn.codingType = VP8ENC_INTRA_FRAME;
  enc->encIn.timeIncrement = 0;

  enc->priv_data = (uint8_t *)calloc(1, 5487);
  if (enc->priv_data == NULL) {
    VPU_PLG_ERR("allocate private data buffer failed\n");
    CloseEncoder(enc->encoder);
    return -1;
  }

  enc->hdr_idx = 0;
  enc->priv_offset = 0;
  return 0;
}

static void rk_vp8_encoder_setconfig(struct rk_vp8_encoder *enc,
                                    struct rk_vepu_runtime_param *param)
{
  int ret;
  vp8Instance_s *pEncInst = (vp8Instance_s *)enc->encoder;
  VP8EncRateCtrl rc;
  pEncInst->encStatus = VP8ENCSTAT_INIT;
  VP8EncGetRateCtrl(enc->encoder, &rc);
  if (param->bitrate != 0) {
    rc.bitPerSecond = param->bitrate;
    rc.pictureRc = ENCHW_YES;
    rc.qpHdr = -1;
  }

  if (param->framerate_denom != 0 && param->framerate_numer != 0) {
    pEncInst->rateControl.outRateNum = param->framerate_numer;
    pEncInst->rateControl.outRateDenom = param->framerate_denom;
    enc->cmdl.outputRateDenom = param->framerate_denom;
    enc->cmdl.outputRateNumer = param->framerate_numer;
  }

  ret = VP8EncSetRateCtrl(enc->encoder, &rc);
  if (ret < 0) {
    VPU_PLG_ERR("failed to set rate control\n");
  }

  VPU_PLG_INF("Reset bitrate calculation parameters\n");
  enc->cmdl.streamSize = 0;
  enc->cmdl.frameCntTotal = 0;

  if (param->keyframe_request) {
    enc->cmdl.keyframeRequest = true;
  }
}

static int rk_vp8_encoder_after_encode(struct rk_vp8_encoder *enc,
                                      uint32_t outputStreamSize) {
  VP8EncRet ret;
  EncoderParameters* cml = &enc->cmdl;

  ret = VP8EncStrmEncodeResult(enc->encoder, &cml->encOut, outputStreamSize);

  VP8EncGetRateCtrl(enc->encoder, &cml->rc);

  cml->streamSize += cml->encOut.frameSize;

  MaAddFrame(&cml->ma, cml->encOut.frameSize * 8);

  enc->encIn.timeIncrement = cml->outputRateDenom;

  cml->frameCnt++;
  cml->frameCntTotal++;

  if (cml->encOut.codingType != VP8ENC_NOTCODED_FRAME) {
    enc->intraPeriodCnt++;
    enc->codedFrameCnt++;
  }

  /* calculate the bitrate using output stream size. */
  if ((cml->frameCntTotal + 1) && cml->outputRateDenom) {
    /* Using 64-bits to avoid overflow */
    uint64_t tmp = cml->streamSize / (cml->frameCntTotal + 1);
    tmp *= (uint32_t)cml->outputRateNumer;

    cml->bitrate = (uint32_t)(8 * (tmp / (uint32_t)cml->outputRateDenom));
  }

  /* Print information about encoded frames */
  VPU_PLG_INF("\nBitrate target %d bps, actual %d bps (%d%%).\n",
              cml->rc.bitPerSecond, cml->bitrate,
              (cml->rc.bitPerSecond) ?
              cml->bitrate * 100 / cml->rc.bitPerSecond : 0);
  VPU_PLG_INF("Total of %llu frames processed, %d frames encoded, %d bytes.\n",
              cml->frameCntTotal, enc->codedFrameCnt, cml->streamSize);

  if (cml->psnrCnt)
    VPU_PLG_INF("Average PSNR %d.%02d\n",
                (cml->psnrSum / cml->psnrCnt) / 100,
                (cml->psnrSum / cml->psnrCnt) % 100);

  return ret;
}

static int rk_vp8_encoder_before_encode(struct rk_vp8_encoder *enc) {
  int ret = 0;
  EncoderParameters* cml = &enc->cmdl;

  cml->ma.pos = cml->ma.count = 0;
  cml->ma.frameRateNumer = cml->outputRateNumer;
  cml->ma.frameRateDenom = cml->outputRateDenom;
  if (cml->outputRateDenom)
    cml->ma.length = MAX(1, MIN(cml->outputRateNumer / cml->outputRateDenom,
                                MOVING_AVERAGE_FRAMES));
  else
    cml->ma.length = MOVING_AVERAGE_FRAMES;

  /* Source Image Size */
  if (cml->inputFormat <= 1) {
    enc->src_img_size = cml->lumWidthSrc * cml->lumHeightSrc +
        2 * (((cml->lumWidthSrc + 1) >> 1) *
             ((cml->lumHeightSrc + 1) >> 1));
  } else if ((cml->inputFormat <= 9) || (cml->inputFormat == 14)) {
    /* 422 YUV or 16-bit RGB */
    enc->src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 2;
  } else {
    /* 32-bit RGB */
    enc->src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 4;
  }

  if (VP8EncGetRateCtrl(enc->encoder, &cml->rc) < 0) {
    VPU_PLG_ERR("VP8 Get Rate Control failed\n");
    return -1;
  }

  /* Code keyframe according to intra period counter
   * or if requested by client. */
  if (cml->keyframeRequest ||
      (cml->intraPicRate != 0 &&
       enc->intraPeriodCnt >= cml->intraPicRate)) {
    enc->encIn.codingType = VP8ENC_INTRA_FRAME;
    enc->intraPeriodCnt = 0;
    cml->keyframeRequest = false;
  } else {
    enc->encIn.codingType = VP8ENC_PREDICTED_FRAME;
  }

  /* This applies for PREDICTED frames only. By default always predict
   * from the previous frame only. */
  enc->encIn.ipf = VP8ENC_REFERENCE_AND_REFRESH;
  enc->encIn.grf = enc->encIn.arf = VP8ENC_REFERENCE;

  /* Encode one frame */
  ret = VP8EncStrmEncode(enc->encoder, &enc->encIn, &cml->encOut, cml);
  if (ret < 0) {
    VPU_PLG_ERR("Generate Encoder configuration failed\n");
    return -1;
  }

  VP8EncGetFrameHeader(enc->encoder, (uint8_t**)&enc->rk_payloads[0],
                       &enc->rk_payload_sizes[0]);
  VP8EncGetRegs(enc->encoder, (uint32_t**)&enc->rk_payloads[1],
                &enc->rk_payload_sizes[1]);
  rk_vp8_encoder_getpriv(
      enc, (uint8_t**)&enc->rk_payloads[2], &enc->rk_payload_sizes[2]);
  return 0;
}

static void rk_vp8_encoder_store_priv_data(struct rk_vp8_encoder *enc,
                                          uint8_t *priv, uint32_t size,
                                          int32_t type) {
  uint32_t* hdr = (uint32_t*)enc->priv_data;
  uint8_t* data = (uint8_t*)enc->priv_data + PRIVATE_DATA_HEADER_SIZE;

  VPU_PLG_DBG("store type %02x, size %02x\n", type, size);
  enc->priv_offset = (enc->priv_offset + 7) & (~7);

  hdr[enc->hdr_idx++] = type;
  hdr[enc->hdr_idx++] = size;
  hdr[enc->hdr_idx++] = enc->priv_offset;

  memcpy(data + enc->priv_offset, priv, size);
  enc->priv_offset += size;
}

static void rk_vp8_encoder_genpriv(struct rk_vp8_encoder *enc) {
  uint8_t *priv;
  uint32_t size;

  enc->hdr_idx = 0;
  enc->priv_offset = 0;
  VP8EncGetCabacCtx(enc->encoder, &priv, &size);
  rk_vp8_encoder_store_priv_data(enc, priv, size, VP8E_PRIVATE_DATA_TYPE_CABAC);
  VP8EncGetSegmentMap(enc->encoder, &priv, &size);
  rk_vp8_encoder_store_priv_data(enc, priv, size, VP8E_PRIVATE_DATA_TYPE_SEGMAP);
  rk_vp8_encoder_store_priv_data(enc, NULL, 0, VP8E_PRIVATE_DATA_TYPE_END);
}

static void rk_vp8_encoder_getpriv(struct rk_vp8_encoder *enc,
                                 uint8_t **priv_data, uint32_t *size) {
  rk_vp8_encoder_genpriv(enc);
  *priv_data = enc->priv_data;
  *size = 5487;
}

static int rk_vp8_encoder_updatepriv(struct rk_vp8_encoder *enc,
                                    void *config, uint32_t cfglen) {
  return VP8EncSetProbCnt(enc->encoder, (uint8_t *)config, cfglen);
}

static void rk_vp8_encoder_deinit(struct rk_vp8_encoder *enc) {
  free(enc->priv_data);
  enc->priv_data = NULL;
  CloseEncoder(enc->encoder);
}

/*----------------------------------------------------------------------------

    OpenEncoder
        Create and configure an encoder instance.

    Params:
        cml     - processed command line options
        pEnc    - place where to save the new encoder instance
    Return:
        0   - for success
        -1  - error

-----------------------------------------------------------------------------*/
static int OpenEncoder(EncoderParameters* cml, VP8EncInst* pEnc) {
  VP8EncRet ret;
  VP8EncConfig cfg;
  VP8EncCodingCtrl codingCfg;
  VP8EncRateCtrl rcCfg;

  VP8EncInst encoder;

  /* input resolution == encoded resolution if not defined */
  if (cml->width == DEFAULT)
    cml->width = cml->lumWidthSrc;
  if (cml->height == DEFAULT)
    cml->height = cml->lumHeightSrc;

  if (cml->rotation) {
    cfg.width = cml->height;
    cfg.height = cml->width;
  } else {
    cfg.width = cml->width;
    cfg.height = cml->height;
  }

  cfg.frameRateDenom = cml->outputRateDenom;
  cfg.frameRateNum = cml->outputRateNumer;
  cfg.refFrameAmount = cml->refFrameAmount;

  VPU_PLG_DBG("Init config: size %dx%d   %d/%d fps  %d refFrames\n",
         cfg.width, cfg.height, cfg.frameRateNum, cfg.frameRateDenom,
         cfg.refFrameAmount);

  if ((ret = VP8EncInit(&cfg, pEnc)) != VP8ENC_OK) {
    VPU_PLG_ERR("VP8EncInit() failed. ret %d\n", ret);
    return ret;
  }

  encoder = *pEnc;

  /* Encoder setup: rate control */
  if ((ret = VP8EncGetRateCtrl(encoder, &rcCfg)) != VP8ENC_OK) {
    VPU_PLG_ERR("VP8EncGetRateCtrl() failed. ret %d\n", ret);
    CloseEncoder(encoder);
    return -1;
  }

  VPU_PLG_INF("Get rate control: qp=%2d [%2d..%2d] %8d bps,"
              " picRc=%d gop=%d\n",
              rcCfg.qpHdr, rcCfg.qpMin, rcCfg.qpMax, rcCfg.bitPerSecond,
              rcCfg.pictureRc, rcCfg.bitrateWindow);

  if (cml->picRc != DEFAULT)
    rcCfg.pictureRc = cml->picRc;
  if (cml->picSkip != DEFAULT)
    rcCfg.pictureSkip = cml->picSkip;
  if (cml->qpHdr != DEFAULT)
    rcCfg.qpHdr = cml->qpHdr;
  if (cml->qpMin != DEFAULT)
    rcCfg.qpMin = cml->qpMin;
  if (cml->qpMax != DEFAULT)
    rcCfg.qpMax = cml->qpMax;
  if (cml->bitPerSecond != DEFAULT)
    rcCfg.bitPerSecond = cml->bitPerSecond;
  if (cml->gopLength != DEFAULT)
    rcCfg.bitrateWindow = cml->gopLength;
  if (cml->intraQpDelta != DEFAULT)
    rcCfg.intraQpDelta = cml->intraQpDelta;
  if (cml->fixedIntraQp != DEFAULT)
    rcCfg.fixedIntraQp = cml->fixedIntraQp;

  VPU_PLG_DBG("Set rate control: qp=%2d [%2d..%2d] %8d bps,"
         " picRc=%d gop=%d\n",
         rcCfg.qpHdr, rcCfg.qpMin, rcCfg.qpMax, rcCfg.bitPerSecond,
         rcCfg.pictureRc, rcCfg.bitrateWindow);

  if ((ret = VP8EncSetRateCtrl(encoder, &rcCfg)) != VP8ENC_OK) {
    VPU_PLG_ERR("VP8EncSetRateCtrl() failed. ret %d\n", ret);
    CloseEncoder(encoder);
    return -1;
  }

  /* Encoder setup: coding control */
  if ((ret = VP8EncGetCodingCtrl(encoder, &codingCfg)) != VP8ENC_OK) {
    VPU_PLG_ERR("VP8EncGetCodingCtrl() failed. ret %d\n", ret);
    CloseEncoder(encoder);
    return -1;
  }

  if (cml->dctPartitions != DEFAULT)
    codingCfg.dctPartitions = cml->dctPartitions;
  if (cml->errorResilient != DEFAULT)
    codingCfg.errorResilient = cml->errorResilient;
  if (cml->ipolFilter != DEFAULT)
    codingCfg.interpolationFilter = cml->ipolFilter;
  if (cml->filterType != DEFAULT)
    codingCfg.filterType = cml->filterType;
  if (cml->filterLevel != DEFAULT)
    codingCfg.filterLevel = cml->filterLevel;
  if (cml->filterSharpness != DEFAULT)
    codingCfg.filterSharpness = cml->filterSharpness;
  if (cml->quarterPixelMv != DEFAULT)
    codingCfg.quarterPixelMv = cml->quarterPixelMv;
  if (cml->splitMv != DEFAULT)
    codingCfg.splitMv = cml->splitMv;

  codingCfg.cirStart = cml->cirStart;
  codingCfg.cirInterval = cml->cirInterval;
  codingCfg.intraArea.enable = cml->intraAreaEnable;
  codingCfg.intraArea.top = cml->intraAreaTop;
  codingCfg.intraArea.left = cml->intraAreaLeft;
  codingCfg.intraArea.bottom = cml->intraAreaBottom;
  codingCfg.intraArea.right = cml->intraAreaRight;
  codingCfg.roi1Area.enable = cml->roi1AreaEnable;
  codingCfg.roi1Area.top = cml->roi1AreaTop;
  codingCfg.roi1Area.left = cml->roi1AreaLeft;
  codingCfg.roi1Area.bottom = cml->roi1AreaBottom;
  codingCfg.roi1Area.right = cml->roi1AreaRight;
  codingCfg.roi2Area.enable = cml->roi2AreaEnable;
  codingCfg.roi2Area.top = cml->roi2AreaTop;
  codingCfg.roi2Area.left = cml->roi2AreaLeft;
  codingCfg.roi2Area.bottom = cml->roi2AreaBottom;
  codingCfg.roi2Area.right = cml->roi2AreaRight;
  codingCfg.roi1DeltaQp = cml->roi1DeltaQp;
  codingCfg.roi2DeltaQp = cml->roi2DeltaQp;

  VPU_PLG_DBG("Set coding control: dctPartitions=%d ipolFilter=%d"
              " errorResilient=%d\n"
              " filterType=%d filterLevel=%d filterSharpness=%d"
              " quarterPixelMv=%d"
              " splitMv=%d\n",
              codingCfg.dctPartitions, codingCfg.interpolationFilter,
              codingCfg.errorResilient, codingCfg.filterType,
              codingCfg.filterLevel, codingCfg.filterSharpness,
              codingCfg.quarterPixelMv, codingCfg.splitMv);

  if (codingCfg.cirInterval)
    VPU_PLG_DBG("  CIR: %d %d\n",
                codingCfg.cirStart, codingCfg.cirInterval);

  if (codingCfg.intraArea.enable)
    VPU_PLG_DBG("  IntraArea: %dx%d-%dx%d\n",
                codingCfg.intraArea.left, codingCfg.intraArea.top,
                codingCfg.intraArea.right, codingCfg.intraArea.bottom);

  if (codingCfg.roi1Area.enable)
    VPU_PLG_DBG("  ROI 1: %d  %dx%d-%dx%d\n", codingCfg.roi1DeltaQp,
                codingCfg.roi1Area.left, codingCfg.roi1Area.top,
                codingCfg.roi1Area.right, codingCfg.roi1Area.bottom);

  if (codingCfg.roi2Area.enable)
    VPU_PLG_DBG("  ROI 2: %d  %dx%d-%dx%d\n", codingCfg.roi2DeltaQp,
                codingCfg.roi2Area.left, codingCfg.roi2Area.top,
                codingCfg.roi2Area.right, codingCfg.roi2Area.bottom);


  if ((ret = VP8EncSetCodingCtrl(encoder, &codingCfg)) != VP8ENC_OK) {
    VPU_PLG_ERR("VP8EncSetCodingCtrl() failed. ret %d\n", ret);
    CloseEncoder(encoder);
    return -1;
  }

  return 0;
}

/*------------------------------------------------------------------------------

    CloseEncoder
       Release an encoder insatnce.

   Params:
        encoder - the instance to be released
------------------------------------------------------------------------------*/
static void CloseEncoder(VP8EncInst encoder) {
  VP8EncRet ret;

  if ((ret = VP8EncRelease(encoder)) != VP8ENC_OK)
    VPU_PLG_ERR("VP8EncRelease() failed. ret %d\n", ret);
}

void SetDefaultParameter(EncoderParameters* cml) {
  memset(cml, 0, sizeof(EncoderParameters));

  /* Default setting tries to parse resolution from file name */

  /* Width of encoded output image */
  cml->width              = 176;
  /* Height of encoded output image */
  cml->height             = 144;
  /* Width of source image [176] */
  cml->lumWidthSrc        = 176;
  /* Height of source image [144] */
  cml->lumHeightSrc       = 144;
  /* Output image horizontal cropping offset [0] */
  cml->horOffsetSrc       = 0;
  /* Output image vertical cropping offset [0] */
  cml->verOffsetSrc       = 0;

  /* Input YUV format [1] */
  cml->inputFormat        = VP8ENC_YUV420_SEMIPLANAR;
  /* 1..1048575 Output picture rate numerator. [--inputRateNumer] */
  cml->outputRateNumer    = 30;
  /* 1..1048575 Output picture rate denominator. [--inputRateDenom] */
  cml->outputRateDenom    = 1;
  /* 1..3 Amount of buffered reference frames. [1] */
  cml->refFrameAmount     = 1;

  /* Default settings are get from API and not changed in testbench */

  /* RGB to YCbCr color conversion type. [0] */
  cml->colorConversion    = VP8ENC_RGBTOYUV_BT601;
  /* Enable video stabilization or scene change detection. [0] */
  cml->videoStab          = 0;
  /* Rotate input image. [0] */
  cml->rotation           = 0;

  /* -1..127, Initial QP used for the first frame. [36] */
  cml->qpHdr              = 36;
  /* 0..127, Minimum frame header QP. [10] */
  cml->qpMin              = 0;
  /* 0..127, Maximum frame header QP. [51] */
  cml->qpMax              = QINDEX_RANGE - 1;
  /* 10000..60000000, Target bitrate for rate control [1000000] */
  cml->bitPerSecond       = 1000000;
  /* 0=OFF, 1=ON, Picture rate control enable. [1] */
  cml->picRc              = 0;
  /* 0=OFF, 1=ON, Picture skip rate control. [0] */
  cml->picSkip            = 0;
  /* -12..12, Intra QP delta. [0] */
  cml->intraQpDelta       = 0;
  /* 0..127, Fixed Intra QP, 0 = disabled. [0] */
  cml->fixedIntraQp       = 0;

  /* Intra picture rate in frames. [0] */
  cml->intraPicRate       = 150;
  /* 1..300, Group Of Pictures length in frames. [--intraPicRate] */
  cml->gopLength          = cml->intraPicRate;
  /* 0=1, 1=2, 2=4, 3=8, Amount of DCT partitions to create */
  cml->dctPartitions      = 0;
  /* Enable error resilient stream mode. [0] */
  cml->errorResilient     = 0;
  /* 0=Bicubic, 1=Bilinear, 2=None, Interpolation filter mode. [1] */
  cml->ipolFilter         = 1;
  /* 0=Normal, 1=Simple, Type of in-loop deblocking filter. [0] */
  cml->filterType         = 0;
  /* 0..64, 64=auto, Filter strength level for deblocking. [64] */
  cml->filterLevel        = 64;
  /* 0..8, 8=auto, Filter sharpness for deblocking. [8] */
  cml->filterSharpness    = 8;
  /* 0=OFF, 1=Adaptive, 2=ON, use 1/4 pixel MVs. [1] */
  cml->quarterPixelMv     = 1;
  /* 0=OFF, 1=Adaptive, 2=ON, allowed to to use more than 1 MV/MB. [1] */
  cml->splitMv            = 1;

  /* start:interval for Cyclic Intra Refresh, forces MBs intra */
  cml->cirStart           = 0;
  /* start:interval for Cyclic Intra Refresh, forces MBs intra */
  cml->cirInterval        = 0;

  /* left:top:right:bottom macroblock coordinates */
  cml->intraAreaLeft      = 0;
  cml->intraAreaTop       = 0;
  cml->intraAreaRight     = 0;
  cml->intraAreaBottom    = 0;
  cml->intraAreaEnable    = 0;

  /* left:top:right:bottom macroblock coordinates */
  cml->roi1AreaLeft      = 0;
  cml->roi1AreaTop       = 0;
  cml->roi1AreaRight     = 0;
  cml->roi1AreaBottom    = 0;
  cml->roi1AreaEnable    = 0;
  cml->roi2AreaLeft      = 0;
  cml->roi2AreaTop       = 0;
  cml->roi2AreaRight     = 0;
  cml->roi2AreaBottom    = 0;
  cml->roi2AreaEnable    = 0;

  /* QP delta value for 1st Region-Of-Interest. [-50,0] */
  cml->roi1DeltaQp    = 0;
  cml->roi2DeltaQp    = 0;
  cml->roi1AreaEnable = cml->roi1DeltaQp;
  cml->roi2AreaEnable = cml->roi2DeltaQp;

  /* Enables PSNR calculation for each frame. [0] */
  cml->psnrSum        = 0;
  cml->psnrCnt        = 0;

  /* Enable MV writing in <mv.txt> [0] */
  cml->mvOutput           = 0;

  /* Favor value for I16x16 mode in I16/I4 */
  cml->intra16Favor       = 0;
  /* Penalty value for intra mode in intra/inter */
  cml->intraPenalty       = 0;
}

void PrintTitle(EncoderParameters* cml) {
  VPU_PLG_DBG("\n");
  VPU_PLG_DBG("Input | Pic |  QP | Type | IP GR AR |   "
         "BR avg    MA(%3d) | ByteCnt (inst) |",
         cml->ma.length);

  if (cml->printPsnr)
    VPU_PLG_DBG(" PSNR  |");

  VPU_PLG_DBG("\n");
  VPU_PLG_DBG("----------------------------------------"
         "-----------------------------------------\n");

  VPU_PLG_DBG("      |     | %3d | HDR  |          |   "
         "                  | %7i %6i |",
         cml->rc.qpHdr, cml->streamSize, IVF_HDR_BYTES);

  if (cml->printPsnr)
    VPU_PLG_DBG("       |");
  VPU_PLG_DBG("\n");
}

void PrintFrame(EncoderParameters* cml, VP8EncInst encoder,
                uint32_t frameNumber,
                VP8EncRet ret) {
  if ((cml->frameCntTotal + 1) && cml->outputRateDenom) {
    /* Using 64-bits to avoid overflow */
    uint64_t tmp = cml->streamSize / (cml->frameCntTotal + 1);
    tmp *= (uint32_t)cml->outputRateNumer;

    cml->bitrate = (uint32_t)(8 * (tmp / (uint32_t)cml->outputRateDenom));
  }

  VPU_PLG_DBG("%5i | %3llu | %3d | ",
         frameNumber, cml->frameCntTotal, cml->rc.qpHdr);

  VPU_PLG_DBG("%s",
         (ret == VP8ENC_OUTPUT_BUFFER_OVERFLOW) ?
         "lost" : (cml->encOut.codingType == VP8ENC_INTRA_FRAME) ? " I  " :
         (cml->encOut.codingType == VP8ENC_PREDICTED_FRAME) ? " P  " : "skip");

  /* Print reference frame usage */
  VPU_PLG_DBG(" | %c%c %c%c %c%c",
         cml->encOut.ipf & VP8ENC_REFERENCE ? 'R' : ' ',
         cml->encOut.ipf & VP8ENC_REFRESH   ? 'W' : ' ',
         cml->encOut.grf & VP8ENC_REFERENCE ? 'R' : ' ',
         cml->encOut.grf & VP8ENC_REFRESH   ? 'W' : ' ',
         cml->encOut.arf & VP8ENC_REFERENCE ? 'R' : ' ',
         cml->encOut.arf & VP8ENC_REFRESH   ? 'W' : ' ');

  /* Print bitrate statistics and frame size */
  VPU_PLG_DBG(" | %9u %9u | %7i %6i | ",
         cml->bitrate, Ma(&cml->ma), cml->streamSize, cml->encOut.frameSize);

  /* Print size of each partition in bytes */
  VPU_PLG_DBG("%d %d %d %d\n", cml->encOut.frameSize ? IVF_FRM_BYTES : 0,
         cml->encOut.streamSize[0],
         cml->encOut.streamSize[1], cml->encOut.streamSize[2]);

  /* Check that partition sizes match frame size */
  if (cml->encOut.frameSize != (cml->encOut.streamSize[0] +
                                cml->encOut.streamSize[1] +
                                cml->encOut.streamSize[2])) {
    VPU_PLG_DBG("ERROR: Frame size doesn't match partition sizes!\n");
  }
}

/*----------------------------------------------------------------------------
    Add new frame bits for moving average bitrate calculation
-----------------------------------------------------------------------------*/
static void MaAddFrame(ma_s* ma, int32_t frameSizeBits) {
  ma->frame[ma->pos++] = frameSizeBits;

  if (ma->pos == ma->length)
    ma->pos = 0;

  if (ma->count < ma->length)
    ma->count++;
}

/*----------------------------------------------------------------------------
    Calculate average bitrate of moving window
-----------------------------------------------------------------------------*/
static int32_t Ma(ma_s* ma) {
  int32_t i;
  uint64_t sum = 0;     /* Using 64-bits to avoid overflow */

  for (i = 0; i < ma->count; i++)
    sum += ma->frame[i];

  if (!ma->frameRateDenom)
    return 0;

  sum = sum / ma->length;

  return sum * ma->frameRateNumer / ma->frameRateDenom;
}

static struct rk_venc_ops vp8_enc_ops = {
  .init = rk_vp8_encoder_init,
  .before_encode = rk_vp8_encoder_before_encode,
  .after_encode = rk_vp8_encoder_after_encode,
  .deinit = rk_vp8_encoder_deinit,
  .updatepriv = rk_vp8_encoder_updatepriv,
  .updateparameter = rk_vp8_encoder_setconfig,
};

struct rk_vp8_encoder* rk_vp8_encoder_alloc_ctx(void)
{
  struct rk_vp8_encoder* enc =
    (struct rk_vp8_encoder*)calloc(1, sizeof(struct rk_vp8_encoder));

  if (enc == NULL) {
    VPU_PLG_ERR("allocate decoder context failed\n");
    return NULL;
  }

  enc->ops = &vp8_enc_ops;

  return enc;
}

void rk_vp8_encoder_free_ctx(struct rk_vp8_encoder *enc)
{
  free(enc);
}

