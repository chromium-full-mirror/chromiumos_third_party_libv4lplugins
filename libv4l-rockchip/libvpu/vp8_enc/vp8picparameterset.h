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

#ifndef VP8PIC_PARAMETER_SET_H
#define VP8PIC_PARAMETER_SET_H

#include <stdbool.h>
#include <stdint.h>

#define SGM_CNT 4

typedef struct sgm {
  bool mapModified;   /* Segmentation map has been modified */
  int32_t  idCnt[SGM_CNT];    /* Id counts because of probability */
  /* Segment ID map is stored in ASIC SW/HW mem regs->segmentMap */
} sgm;

typedef struct {
  struct sgm sgm;     /* Segmentation data */
  int32_t qp;         /* Final qp value of current macroblock */
  bool segmentEnabled;    /* Segmentation enabled */
  int32_t qpSgm[SGM_CNT]; /* Qp if segments enabled (encoder set) */
  int32_t levelSgm[SGM_CNT];  /* Level if segments enabled (encoder set) */
} pps;

typedef struct {
  pps* store;     /* Picture parameter set tables */
  int32_t size;       /* Size of above storage table */
  pps* pps;       /* Active picture parameter set */
  pps* prevPps;       /* Previous picture parameter set */
  int32_t qpSgm[SGM_CNT]; /* Current qp and level of segmentation... */
  int32_t levelSgm[SGM_CNT];  /* ...which are written to the stream */
} ppss;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/
int32_t PicParameterSetAlloc(ppss* ppss);
void PicParameterSetFree(ppss* ppss);

#endif
