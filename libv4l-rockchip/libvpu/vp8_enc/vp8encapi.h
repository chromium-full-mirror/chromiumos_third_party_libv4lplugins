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

#ifndef _VP8ENCAPI_H_
#define _VP8ENCAPI_H_

#include "vpu_mem.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* The maximum amount of frames for bitrate moving average calculation */
#define MOVING_AVERAGE_FRAMES    30

typedef const void* VP8EncInst;

/* Function return values */
typedef enum
{
  VP8ENC_OK = 0,
  VP8ENC_FRAME_READY = 1,

  VP8ENC_ERROR = -1,
  VP8ENC_NULL_ARGUMENT = -2,
  VP8ENC_INVALID_ARGUMENT = -3,
  VP8ENC_MEMORY_ERROR = -4,
  VP8ENC_EWL_ERROR = -5,
  VP8ENC_EWL_MEMORY_ERROR = -6,
  VP8ENC_INVALID_STATUS = -7,
  VP8ENC_OUTPUT_BUFFER_OVERFLOW = -8,
  VP8ENC_HW_BUS_ERROR = -9,
  VP8ENC_HW_DATA_ERROR = -10,
  VP8ENC_HW_TIMEOUT = -11,
  VP8ENC_HW_RESERVED = -12,
  VP8ENC_SYSTEM_ERROR = -13,
  VP8ENC_INSTANCE_ERROR = -14,
  VP8ENC_HRD_ERROR = -15,
  VP8ENC_HW_RESET = -16
} VP8EncRet;

/* Picture YUV type for pre-processing */
typedef enum
{
  VP8ENC_YUV420_PLANAR = 0,               /* YYYY... UUUU... VVVV */
  VP8ENC_YUV420_SEMIPLANAR = 1,           /* YYYY... UVUVUV...    */
  VP8ENC_YUV422_INTERLEAVED_YUYV = 2,     /* YUYVYUYV...          */
  VP8ENC_YUV422_INTERLEAVED_UYVY = 3,     /* UYVYUYVY...          */
  VP8ENC_RGB565 = 4,                      /* 16-bit RGB           */
  VP8ENC_BGR565 = 5,                      /* 16-bit RGB           */
  VP8ENC_RGB555 = 6,                      /* 15-bit RGB           */
  VP8ENC_BGR555 = 7,                      /* 15-bit RGB           */
  VP8ENC_RGB444 = 8,                      /* 12-bit RGB           */
  VP8ENC_BGR444 = 9,                      /* 12-bit RGB           */
  VP8ENC_RGB888 = 10,                     /* 24-bit RGB           */
  VP8ENC_BGR888 = 11,                     /* 24-bit RGB           */
  VP8ENC_RGB101010 = 12,                  /* 30-bit RGB           */
  VP8ENC_BGR101010 = 13                   /* 30-bit RGB           */
} VP8EncPictureType;

/* Picture rotation for pre-processing */
typedef enum
{
  VP8ENC_ROTATE_0 = 0,
  VP8ENC_ROTATE_90R = 1, /* Rotate 90 degrees clockwise           */
  VP8ENC_ROTATE_90L = 2  /* Rotate 90 degrees counter-clockwise   */
} VP8EncPictureRotation;

/* Picture color space conversion (RGB input) for pre-processing */
typedef enum
{
  VP8ENC_RGBTOYUV_BT601 = 0, /* Color conversion according to BT.601  */
  VP8ENC_RGBTOYUV_BT709 = 1, /* Color conversion according to BT.709  */
  VP8ENC_RGBTOYUV_USER_DEFINED = 2   /* User defined color conversion */
} VP8EncColorConversionType;

/* Picture type for encoding */
typedef enum
{
  VP8ENC_INTRA_FRAME = 0,
  VP8ENC_PREDICTED_FRAME = 1,
  VP8ENC_NOTCODED_FRAME = 2              /* Used just as a return value  */
} VP8EncPictureCodingType;

/* Reference picture mode for reading and writing */
typedef enum
{
  VP8ENC_NO_REFERENCE_NO_REFRESH = 0,
  VP8ENC_REFERENCE = 1,
  VP8ENC_REFRESH = 2,
  VP8ENC_REFERENCE_AND_REFRESH = 3
} VP8EncRefPictureMode;

/* Definitions to enable encoder internal control of filter parameters */
#define VP8ENC_FILTER_SHARPNESS_AUTO 8
#define VP8ENC_FILTER_LEVEL_AUTO 64

/*------------------------------------------------------------------------------
    3. Structures for API function parameters
------------------------------------------------------------------------------*/

/* Configuration info for initialization
 * Width and height are picture dimensions after rotation
 */
typedef struct
{
  uint32_t refFrameAmount; /* Amount of reference frame buffers, [1..3]
                       * 1 = only last frame buffered,
                       *     always predict from and refresh ipf,
                       *     stream buffer overflow causes new key frame
                       * 2 = last and golden frames buffered
                       * 3 = last and golden and altref frame buffered  */
  uint32_t width;          /* Encoded picture width in pixels, multiple of 4 */
  uint32_t height;         /* Encoded picture height in pixels, multiple of 2*/
  uint32_t frameRateNum;   /* The stream time scale, [1..1048575]            */
  uint32_t frameRateDenom; /* Maximum frame rate is frameRateNum/frameRateDenom
                       * in frames/second. [1..frameRateNum]            */
} VP8EncConfig;

/* Defining rectangular macroblock area in encoded picture */
typedef struct
{
  uint32_t enable;         /* [0,1] Enables this area */
  uint32_t top;            /* Top mb row inside area [0..heightMbs-1]      */
  uint32_t left;           /* Left mb row inside area [0..widthMbs-1]      */
  uint32_t bottom;         /* Bottom mb row inside area [top..heightMbs-1] */
  uint32_t right;          /* Right mb row inside area [left..widthMbs-1]  */
} VP8EncPictureArea;

/* Coding control parameters */
typedef struct
{
  /* The following parameters can only be adjusted before stream is
   * started. They affect the stream profile and decoding complexity. */
  uint32_t interpolationFilter; /* Defines the type of interpolation filter
                            * for reconstruction. [0..2]
                            * 0 = Bicubic (6-tap interpolation filter),
                            * 1 = Bilinear (2-tap interpolation filter),
                            * 2 = None (Full pixel motion vectors)     */
  uint32_t filterType;         /* Deblocking loop filter type,
                           * 0 = Normal deblocking filter,
                           * 1 = Simple deblocking filter             */

  /* The following parameters can be adjusted between frames.         */
  uint32_t filterLevel;        /* Deblocking filter level [0..64]
                           * 0 = No filtering,
                           * higher level => more filtering,
                           * VP8ENC_FILTER_LEVEL_AUTO = calculate
                           *      filter level based on quantization  */
  uint32_t filterSharpness;    /* Deblocking filter sharpness [0..8],
                           * VP8ENC_FILTER_SHARPNESS_AUTO = calculate
                           *      filter sharpness automatically      */
  uint32_t dctPartitions;      /* Amount of DCT coefficient partitions to
                           * create for each frame, subject to HW
                           * limitations on maximum partition amount.
                           * 0 = 1 DCT residual partition,
                           * 1 = 2 DCT residual partitions,
                           * 2 = 4 DCT residual partitions,
                           * 3 = 8 DCT residual partitions            */
  uint32_t errorResilient;     /* Enable error resilient stream mode. [0,1]
                           * This prevents cumulative probability
                           * updates.                                 */
  uint32_t splitMv;            /* Split MV mode ie. using more than 1 MV/MB
                           * 0=disabled, 1=adaptive, 2=enabled        */
  uint32_t quarterPixelMv;     /* 1/4 pixel motion estimation
                           * 0=disabled, 1=adaptive, 2=enabled        */
  uint32_t cirStart;           /* [0..mbTotal] First macroblock for
                           * Cyclic Intra Refresh                     */
  uint32_t cirInterval;        /* [0..mbTotal] Macroblock interval for
                           * Cyclic Intra Refresh, 0=disabled         */
  VP8EncPictureArea intraArea;  /* Area for forcing intra macroblocks */
  VP8EncPictureArea roi1Area;   /* Area for 1st Region-Of-Interest */
  VP8EncPictureArea roi2Area;   /* Area for 2nd Region-Of-Interest */
  int32_t roi1DeltaQp;              /* [-50..0] QP delta value for 1st ROI */
  int32_t roi2DeltaQp;              /* [-50..0] QP delta value for 2nd ROI */
} VP8EncCodingCtrl;

/* Rate control parameters */
typedef struct
{
  uint32_t pictureRc;       /* Adjust QP between pictures, [0,1]           */
  uint32_t pictureSkip;     /* Allow rate control to skip pictures, [0,1]  */
  int32_t qpHdr;           /* QP for next encoded picture, [-1..127]
                        * -1 = Let rate control calculate initial QP.
                        * qpHdr is used for all pictures if
                        * pictureRc is disabled.                      */
  uint32_t qpMin;           /* Minimum QP for any picture, [0..127]        */
  uint32_t qpMax;           /* Maximum QP for any picture, [0..127]        */
  uint32_t bitPerSecond;    /* Target bitrate in bits/second
                        * [10000..60000000]                           */
  uint32_t bitrateWindow;   /* Number of pictures over which the target
                        * bitrate should be achieved. Smaller window
                        * maintains constant bitrate but forces rapid
                        * quality changes whereas larger window
                        * allows smoother quality changes. [1..150]   */
  int32_t intraQpDelta;    /* Intra QP delta. intraQP = QP + intraQpDelta
                        * This can be used to change the relative
                        * quality of the Intra pictures or to decrease
                        * the size of Intra pictures. [-12..12]       */
  uint32_t fixedIntraQp;    /* Fixed QP value for all Intra pictures, [0..127]
                        * 0 = Rate control calculates intra QP.       */
  uint32_t intraPictureRate; /* The distance of two intra pictures, [0..300]
                         * This will force periodical intra pictures.
                         * 0=disabled.                                 */
  uint32_t goldenPictureRate; /* The distance of two golden pictures, [0..300]
                         * This will force periodical golden pictures.
                         * 0=disabled.                                 */
  uint32_t altrefPictureRate; /* The distance of two altref pictures, [0..300]
                         * This will force periodical altref pictures.
                         * 0=disabled.                                 */
} VP8EncRateCtrl;

/* Encoder input structure */
typedef struct
{
  uint32_t busLuma;         /* Bus address for input picture
                        * planar format: luminance component
                        * semiplanar format: luminance component
                        * interleaved format: whole picture
                        */
  uint32_t busChromaU;      /* Bus address for input chrominance
                        * planar format: cb component
                        * semiplanar format: both chrominance
                        * interleaved format: not used
                        */
  uint32_t busChromaV;      /* Bus address for input chrominance
                        * planar format: cr component
                        * semiplanar format: not used
                        * interleaved format: not used
                        */
  VP8EncPictureCodingType codingType; /* Proposed picture coding type,
                                       * INTRA/PREDICTED              */
  uint32_t timeIncrement;   /* The previous picture duration in units
                        * of 1/frameRateNum. 0 for the very first
                        * picture and typically equal to frameRateDenom
                        * for the rest.                               */

  /* The following three parameters apply when
   * codingType == PREDICTED. They define for each
   * of the reference pictures if it should be used
   * for prediction and if it should be refreshed
   * with the encoded frame. There must always be
   * atleast one (ipf/grf/arf) that is referenced
   * and atleast one that is refreshed.
   * Note that refFrameAmount may limit the
   * availability of golden and altref frames.   */
  VP8EncRefPictureMode ipf; /* Immediately previous == last frame */
  VP8EncRefPictureMode grf; /* Golden reference frame */
  VP8EncRefPictureMode arf; /* Alternative reference frame */
} VP8EncIn;

/* Encoder output structure */
typedef struct
{
  VP8EncPictureCodingType codingType;     /* Realized picture coding type,
                                           * INTRA/PREDICTED/NOTCODED   */
  uint32_t* pOutBuf[9];     /* Pointers to start of each partition in
                        * output stream buffer,
                        * pOutBuf[0] = Frame header + mb mode partition,
                        * pOutBuf[1] = First DCT partition,
                        * pOutBuf[2] = Second DCT partition (if exists)
                        * etc.                                          */
  uint32_t streamSize[9];   /* Size of each partition of output stream
                        * in bytes.                                     */
  uint32_t frameSize;       /* Size of output frame in bytes
                        * (==sum of partition sizes)                    */
  int8_t* motionVectors;   /* Pointer to buffer storing encoded frame MVs.
                        * One pixel motion vector x and y and
                        * corresponding SAD value for every macroblock.
                        * Format: mb0x mb0y mb0sadMsb mb0sadLsb mb1x .. */

  /* The following three parameters apply when
   * codingType == PREDICTED. They define for each
   * of the reference pictures if it was used for
   * prediction and it it was refreshed. */
  VP8EncRefPictureMode ipf; /* Immediately previous == last frame */
  VP8EncRefPictureMode grf; /* Golden reference frame */
  VP8EncRefPictureMode arf; /* Alternative reference frame */
} VP8EncOut;

/* Input pre-processing */
typedef struct
{
  VP8EncColorConversionType type;
  uint16_t coeffA;          /* User defined color conversion coefficient */
  uint16_t coeffB;          /* User defined color conversion coefficient */
  uint16_t coeffC;          /* User defined color conversion coefficient */
  uint16_t coeffE;          /* User defined color conversion coefficient */
  uint16_t coeffF;          /* User defined color conversion coefficient */
} VP8EncColorConversion;

/* Version information */
typedef struct
{
  uint32_t major;           /* Encoder API major version */
  uint32_t minor;           /* Encoder API minor version */
} VP8EncApiVersion;

typedef struct
{
  uint32_t swBuild;         /* Software build ID */
  uint32_t hwBuild;         /* Hardware build ID */
} VP8EncBuild;


typedef struct {
  int32_t frame[MOVING_AVERAGE_FRAMES];
  int32_t length;
  int32_t count;
  int32_t pos;
  int32_t frameRateNumer;
  int32_t frameRateDenom;
} ma_s;

/* Structure for command line options and testbench variables */
typedef struct
{
  int32_t width;
  int32_t height;
  int32_t lumWidthSrc;
  int32_t lumHeightSrc;
  int32_t horOffsetSrc;
  int32_t verOffsetSrc;
  int32_t outputRateNumer;
  int32_t outputRateDenom;
  int32_t refFrameAmount;

  int32_t inputFormat;
  int32_t rotation;
  int32_t colorConversion;
  int32_t videoStab;

  int32_t bitPerSecond;
  int32_t picRc;
  int32_t picSkip;
  int32_t gopLength;
  int32_t qpHdr;
  int32_t qpMin;
  int32_t qpMax;
  int32_t intraQpDelta;
  int32_t fixedIntraQp;

  int32_t intraPicRate;
  int32_t dctPartitions;  /* Dct data partitions 0=1, 1=2, 2=4, 3=8 */
  int32_t errorResilient;
  int32_t ipolFilter;
  int32_t quarterPixelMv;
  int32_t splitMv;
  int32_t filterType;
  int32_t filterLevel;
  int32_t filterSharpness;
  int32_t cirStart;
  int32_t cirInterval;
  int32_t intraAreaEnable;
  int32_t intraAreaLeft;
  int32_t intraAreaTop;
  int32_t intraAreaRight;
  int32_t intraAreaBottom;
  int32_t roi1AreaEnable;
  int32_t roi2AreaEnable;
  int32_t roi1AreaTop;
  int32_t roi1AreaLeft;
  int32_t roi1AreaBottom;
  int32_t roi1AreaRight;
  int32_t roi2AreaTop;
  int32_t roi2AreaLeft;
  int32_t roi2AreaBottom;
  int32_t roi2AreaRight;
  int32_t roi1DeltaQp;
  int32_t roi2DeltaQp;

  int32_t printPsnr;
  int32_t mvOutput;
  int32_t droppable;

  int32_t intra16Favor;
  int32_t intraPenalty;

  /* SW/HW shared memories for input/output buffers */
  VPUMemLinear_t pictureMem;
  VPUMemLinear_t pictureStabMem;

  VP8EncRateCtrl rc;
  VP8EncOut encOut;

  uint32_t streamSize;     /* Size of output stream in bytes */
  uint32_t bitrate;        /* Calculate average bitrate of encoded frames */
  ma_s ma;            /* Calculate moving average of bitrate */
  uint32_t psnrSum;        /* Calculate average PSNR over encoded frames */
  uint32_t psnrCnt;

  int32_t frameCnt;       /* Frame counter of input file */
  uint64_t frameCntTotal;  /* Frame counter of all frames */
} EncoderParameters;


/* Initialization & release */
VP8EncRet VP8EncInit(const VP8EncConfig* pEncConfig,
                     VP8EncInst* instAddr);
VP8EncRet VP8EncRelease(VP8EncInst inst);

/* Encoder configuration before stream generation */
VP8EncRet VP8EncSetCodingCtrl(VP8EncInst inst, const VP8EncCodingCtrl*
                              pCodingParams);
VP8EncRet VP8EncGetCodingCtrl(VP8EncInst inst, VP8EncCodingCtrl*
                              pCodingParams);

/* Encoder configuration before and during stream generation */
VP8EncRet VP8EncSetRateCtrl(VP8EncInst inst,
                            const VP8EncRateCtrl* pRateCtrl);
VP8EncRet VP8EncGetRateCtrl(VP8EncInst inst,
                            VP8EncRateCtrl* pRateCtrl);

/* Stream generation */

void VP8EncGetFrameHeader(VP8EncInst inst, uint8_t** frmhdr, uint32_t* size);
void VP8EncGetCabacCtx(VP8EncInst inst, uint8_t** cabac, uint32_t* size);
void VP8EncGetSegmentMap(VP8EncInst inst, uint8_t** segmap, uint32_t* size);
void VP8EncGetRegs(VP8EncInst inst, uint32_t** regs, uint32_t* size);
VP8EncRet VP8EncSetProbCnt(VP8EncInst inst, uint8_t* probcnt, uint32_t size);

/* VP8EncStrmEncode encodes one video frame. */
VP8EncRet VP8EncStrmEncodeResult(VP8EncInst inst, VP8EncOut* pEncOut, uint32_t outputStreamSize);
VP8EncRet VP8EncStrmEncode(VP8EncInst inst, const VP8EncIn* pEncIn,
                           VP8EncOut* pEncOut, EncoderParameters* cml);

void PrintTitle(EncoderParameters* cml);
void PrintFrame(EncoderParameters* cml, VP8EncInst encoder, uint32_t frameNumber,
                VP8EncRet ret);

#ifdef __cplusplus
}
#endif

#endif /*__VP8ENCAPI_H__*/
