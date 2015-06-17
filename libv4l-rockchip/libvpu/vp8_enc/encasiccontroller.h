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

#ifndef _ENC_ASIC_CONTROLLER_H_
#define _ENC_ASIC_CONTROLLER_H_

#include <stdint.h>
#include "enccfg.h"
#include "vpu_mem.h"
#include "encswhwregisters.h"

/* HW status register bits */
#define ASIC_STATUS_ALL                 0x1FD

#define ASIC_STATUS_SLICE_READY         0x100
#define ASIC_STATUS_HW_TIMEOUT          0x040
#define ASIC_STATUS_BUFF_FULL           0x020
#define ASIC_STATUS_HW_RESET            0x010
#define ASIC_STATUS_ERROR               0x008
#define ASIC_STATUS_FRAME_READY         0x004

#define ASIC_IRQ_LINE                   0x001

#define ASIC_STATUS_ENABLE              0x001

#define ASIC_H264_BYTE_STREAM           0x00
#define ASIC_H264_NAL_UNIT              0x01

#define ASIC_INPUT_YUV420PLANAR         0x00
#define ASIC_INPUT_YUV420SEMIPLANAR     0x01
#define ASIC_INPUT_YUYV422INTERLEAVED   0x02
#define ASIC_INPUT_UYVY422INTERLEAVED   0x03
#define ASIC_INPUT_RGB565               0x04
#define ASIC_INPUT_RGB555               0x05
#define ASIC_INPUT_RGB444               0x06
#define ASIC_INPUT_RGB888               0x07
#define ASIC_INPUT_RGB101010            0x08
#define ASIC_INPUT_YUYV422TILED         0x09

#define ASIC_PENALTY_UNDEFINED          -1

#define ASIC_PENALTY_TABLE_SIZE         128

typedef enum
{
  ASIC_VP8 = 1,
  ASIC_JPEG = 2,
  ASIC_H264 = 3
} asicCodingType_e;

typedef enum
{
  ASIC_P_16x16 = 0,
  ASIC_P_16x8 = 1,
  ASIC_P_8x16 = 2,
  ASIC_P_8x8 = 3,
  ASIC_I_4x4 = 4,
  ASIC_I_16x16 = 5
} asicMbType_e;

typedef enum
{
  ASIC_INTER = 0,
  ASIC_INTRA = 1,
  ASIC_MVC = 2,
  ASIC_MVC_REF_MOD = 3
} asicFrameCodingType_e;

typedef struct
{
  uint32_t socket;
  uint32_t irqDisable;
  uint32_t irqInterval;
  uint32_t mbsInCol;
  uint32_t mbsInRow;
  uint32_t qp;
  uint32_t qpMin;
  uint32_t qpMax;
  uint32_t constrainedIntraPrediction;
  uint32_t roundingCtrl;
  uint32_t frameCodingType;
  uint32_t codingType;
  uint32_t pixelsOnRow;
  uint32_t xFill;
  uint32_t yFill;
  uint32_t ppsId;
  uint32_t idrPicId;
  uint32_t frameNum;
  uint32_t picInitQp;
  int32_t sliceAlphaOffset;
  int32_t sliceBetaOffset;
  uint32_t filterDisable;
  uint32_t transform8x8Mode;
  uint32_t enableCabac;
  uint32_t cabacInitIdc;
  int32_t chromaQpIndexOffset;
  uint32_t sliceSizeMbRows;
  uint32_t inputImageFormat;
  uint32_t inputImageRotation;
  uint32_t outputStrmFrmTagOffset;
  uint32_t outputStrmBase;
  uint32_t outputStrmSize;
  uint32_t firstFreeBit;
  uint32_t strmStartMSB;
  uint32_t strmStartLSB;
  uint32_t rlcBase;
  uint32_t rlcLimitSpace;
  uint32_t sizeTblBase;
  uint32_t sliceReadyInterrupt;
  uint32_t recWriteDisable;
  uint32_t reconImageId;
  uint32_t internalImageLumBaseW;
  uint32_t internalImageChrBaseW;
  uint32_t internalImageLumBaseR[2];
  uint32_t internalImageChrBaseR[2];
  uint32_t cpDistanceMbs;
  uint32_t* cpTargetResults;
  const uint32_t* cpTarget;
  const int32_t* targetError;
  const int32_t* deltaQp;
  uint32_t rlcCount;
  uint32_t qpSum;
  uint32_t h264StrmMode;   /* 0 - byte stream, 1 - NAL units */
  uint8_t quantTable[8 * 8 * 2];
  uint8_t dmvPenalty[ASIC_PENALTY_TABLE_SIZE];
  uint8_t dmvQpelPenalty[ASIC_PENALTY_TABLE_SIZE];
  uint32_t jpegMode;
  uint32_t jpegSliceEnable;
  uint32_t jpegRestartInterval;
  uint32_t jpegRestartMarker;
  uint32_t regMirror[ASIC_SWREG_AMOUNT];
  uint32_t inputLumaBaseOffsetVert;
  uint32_t h264Inter4x4Disabled;
  uint32_t disableQuarterPixelMv;
  uint32_t vsNextLumaBase;
  uint32_t vsMode;
  uint32_t asicCfgReg;
  int32_t intra16Favor;
  int32_t prevModeFavor;
  int32_t interFavor;
  int32_t skipPenalty;
  int32_t goldenPenalty;
  int32_t diffMvPenalty[3];
  int32_t madQpDelta;
  uint32_t madThreshold;
  uint32_t madCount;
  uint32_t mvcAnchorPicFlag;
  uint32_t mvcPriorityId;
  uint32_t mvcViewId;
  uint32_t mvcTemporalId;
  uint32_t mvcInterViewFlag;
  uint32_t cirStart;
  uint32_t cirInterval;
  uint32_t intraSliceMap1;
  uint32_t intraSliceMap2;
  uint32_t intraSliceMap3;
  uint32_t intraAreaTop;
  uint32_t intraAreaLeft;
  uint32_t intraAreaBottom;
  uint32_t intraAreaRight;
  uint32_t roi1Top;
  uint32_t roi1Left;
  uint32_t roi1Bottom;
  uint32_t roi1Right;
  uint32_t roi2Top;
  uint32_t roi2Left;
  uint32_t roi2Bottom;
  uint32_t roi2Right;
  int32_t roi1DeltaQp;
  int32_t roi2DeltaQp;
  uint32_t mvOutputBase;
  uint32_t cabacCtxBase;
  uint32_t probCountBase;
  uint32_t segmentMapBase;
  uint32_t colorConversionCoeffA;
  uint32_t colorConversionCoeffB;
  uint32_t colorConversionCoeffC;
  uint32_t colorConversionCoeffE;
  uint32_t colorConversionCoeffF;
  uint32_t rMaskMsb;
  uint32_t gMaskMsb;
  uint32_t bMaskMsb;
  uint32_t partitionOffset[8];
  uint32_t partitionBase[8];
  uint32_t qpY1QuantDc[4];
  uint32_t qpY1QuantAc[4];
  uint32_t qpY2QuantDc[4];
  uint32_t qpY2QuantAc[4];
  uint32_t qpChQuantDc[4];
  uint32_t qpChQuantAc[4];
  uint32_t qpY1ZbinDc[4];
  uint32_t qpY1ZbinAc[4];
  uint32_t qpY2ZbinDc[4];
  uint32_t qpY2ZbinAc[4];
  uint32_t qpChZbinDc[4];
  uint32_t qpChZbinAc[4];
  uint32_t qpY1RoundDc[4];
  uint32_t qpY1RoundAc[4];
  uint32_t qpY2RoundDc[4];
  uint32_t qpY2RoundAc[4];
  uint32_t qpChRoundDc[4];
  uint32_t qpChRoundAc[4];
  uint32_t qpY1DequantDc[4];
  uint32_t qpY1DequantAc[4];
  uint32_t qpY2DequantDc[4];
  uint32_t qpY2DequantAc[4];
  uint32_t qpChDequantDc[4];
  uint32_t qpChDequantAc[4];
  uint32_t segmentEnable;
  uint32_t segmentMapUpdate;
  uint32_t mvRefIdx[2];
  uint32_t ref2Enable;
  uint32_t boolEncValue;
  uint32_t boolEncValueBits;
  uint32_t boolEncRange;
  uint32_t dctPartitions;
  uint32_t filterLevel[4];
  uint32_t filterSharpness;
  uint32_t intraModePenalty[4];
  uint32_t intraBmodePenalty[10];
  uint32_t zeroMvFavorDiv2;
  uint32_t splitMvMode;
  uint32_t splitPenalty[4];
  int32_t lfRefDelta[4];
  int32_t lfModeDelta[4];
} regValues_s;

#define FRAME_HEADER_SIZE 1280
typedef struct
{
  regValues_s regs;
  VPUMemLinear_t internalImageLuma[3];
  VPUMemLinear_t internalImageChroma[4];
  VPUMemLinear_t cabacCtx;
  VPUMemLinear_t probCount;
  VPUMemLinear_t segmentMap;
  uint32_t sizeTblSize;
  uint32_t traceRecon;
  uint8_t frmhdr[FRAME_HEADER_SIZE];
  uint32_t frmHdrBufLen;
} asicData_s;

int32_t VP8_EncAsicControllerInit(asicData_s* asic);

int32_t VP8_EncAsicMemAlloc_V2(
    asicData_s* asic, uint32_t width, uint32_t height, uint32_t encodingType,
    uint32_t numRefBuffsLum, uint32_t numRefBuffsChr);

void VP8_EncAsicMemFree_V2(asicData_s* asic);

void VP8_EncAsicFrameStart(regValues_s* val);

#endif
