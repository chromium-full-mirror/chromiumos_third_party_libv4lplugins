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
#ifndef VP8_RATE_CONTROL_H
#define VP8_RATE_CONTROL_H

#include <stdint.h>

#include "enccommon.h"

typedef struct {
  int32_t  a1;               /* model parameter */
  int32_t  a2;               /* model parameter */
  int32_t  qp_prev;          /* previous QP */
  int32_t  qs[15];           /* quantization step size */
  int32_t  bits[15];         /* Number of bits needed to code residual */
  int32_t  pos;              /* current position */
  int32_t  len;              /* current lenght */
  int32_t  zero_div;         /* a1 divisor is 0 */
} linReg_s;

/* Virtual buffer */
typedef struct
{
  int32_t bufferSize;          /* size of the virtual buffer */
  int32_t bitRate;             /* input bit rate per second */
  int32_t bitPerPic;           /* average number of bits per picture */
  int32_t picTimeInc;          /* timeInc since last coded picture */
  int32_t timeScale;           /* input frame rate numerator */
  int32_t unitsInTic;          /* input frame rate denominator */
  int32_t virtualBitCnt;       /* virtual (channel) bit count */
  int32_t realBitCnt;          /* real bit count */
  int32_t bufferOccupancy;     /* number of bits in the buffer */
  int32_t skipFrameTarget;     /* how many frames should be skipped in a row */
  int32_t skippedFrames;       /* how many frames have been skipped in a row */
  int32_t bucketFullness;      /* Leaky Bucket fullness */
  int32_t gopRem;              /* Number of frames remaining in this GOP */
} vp8VirtualBuffer_s;

typedef struct
{
  true_e picRc;
  true_e picSkip;          /* Frame Skip enable */
  true_e frameCoded;       /* Frame coded or not */
  int32_t mbPerPic;            /* Number of macroblock per picture */
  int32_t mbRows;              /* MB rows in picture */
  int32_t currFrameIntra;      /* Is current frame intra frame? */
  int32_t prevFrameIntra;      /* Was previous frame intra frame? */
  int32_t fixedQp;             /* Pic header qp when fixed */
  int32_t qpHdr;               /* Pic header qp of current voded picture */
  int32_t qpMin;               /* Pic header minimum qp, user set */
  int32_t qpMax;               /* Pic header maximum qp, user set */
  int32_t qpHdrPrev;           /* Pic header qp of previous coded picture */
  int32_t outRateNum;
  int32_t outRateDenom;
  vp8VirtualBuffer_s virtualBuffer;
  /* for frame QP rate control */
  linReg_s linReg;       /* Data for R-Q model */
  linReg_s rError;       /* Rate prediction error (bits) */
  int32_t targetPicSize;
  int32_t frameBitCnt;
  /* for GOP rate control */
  int32_t gopQpSum;
  int32_t gopQpDiv;
  int32_t frameCnt;
  int32_t gopLen;
  int32_t intraQpDelta;
  int32_t fixedIntraQp;
  int32_t mbQpAdjustment;     /* QP delta for MAD macroblock QP adjustment */
  int32_t intraPictureRate;
  int32_t goldenPictureRate;
  int32_t altrefPictureRate;
} vp8RateControl_s;

/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/
void VP8InitRc(vp8RateControl_s* rc, uint32_t newStream);
void VP8BeforePicRc(vp8RateControl_s* rc, uint32_t timeInc, uint32_t frameTypeIntra);
void VP8AfterPicRc(vp8RateControl_s* rc, uint32_t byteCnt);
int32_t VP8Calculate(int32_t a, int32_t b, int32_t c);
#endif /* VP8_RATE_CONTROL_H */

