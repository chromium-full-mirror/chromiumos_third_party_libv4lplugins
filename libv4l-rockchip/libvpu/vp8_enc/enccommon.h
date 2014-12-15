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

#ifndef _ENC_COMMON_H_
#define _ENC_COMMON_H_

#define ASSERT(expr)
#define COMMENT(x)
#define TRACE_BIT_STREAM(v,n)

typedef enum
{
  ENCHW_NOK = -1,
  ENCHW_OK = 0
} bool_e;

typedef enum
{
  ENCHW_NO = 0,
  ENCHW_YES = 1
} true_e;

/* General tools */
#define ABS(x)          ((x) < (0) ? -(x) : (x))
#define MAX(a, b)       ((a) > (b) ?  (a) : (b))
#define MIN(a, b)       ((a) < (b) ?  (a) : (b))
#define SIGN(a)         ((a) < (0) ? (-1) : (1))
#define CLIP3(v, min, max)  ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))

#define ALIGN(x, a)      (((x) + (a) - 1) & ~((a) - 1))

#endif
