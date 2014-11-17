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
#include "enccommon.h"
#include "vp8picturebuffer.h"

static void Alloc(refPic* refPic, int32_t width, int32_t height);
static void RefPicListInitialization(picBuffer* ref);
static void ResetRefPic(refPic* refPic);

void PictureBufferAlloc(picBuffer* picBuffer, int32_t width, int32_t height) {
  int32_t i;

  /* Be sure that everything is initialized if something goes wrong */
  memset(picBuffer->refPic, 0, sizeof(picBuffer->refPic));
  memset(picBuffer->refPicList, 0, sizeof(picBuffer->refPicList));

  /* Reference frame base (lum,cb) and macroblock stuff */
  for (i = 0; i < BUFFER_SIZE + 1; i++) {
    Alloc(&picBuffer->refPic[i], width, height);
    /* Back reference pointer (pointer to itself) */
    picBuffer->refPic[i].refPic = &picBuffer->refPic[i];
  }
  picBuffer->cur_pic = &picBuffer->refPic[0];
}

void PictureBufferFree(picBuffer* picBuffer) {
  memset(picBuffer->refPic, 0, sizeof(picBuffer->refPic));
}

void Alloc(refPic* refPic, int32_t width, int32_t height) {
  refPic->picture.lumWidth  = width;
  refPic->picture.lumHeight = height;
  refPic->picture.chWidth   = width / 2;
  refPic->picture.chHeight  = height / 2;
  refPic->picture.lum       = 0;
  refPic->picture.cb        = 0;
}

void InitializePictureBuffer(picBuffer* picBuffer) {
  int32_t i;

  /* I frame (key frame) resets reference pictures */
  if (picBuffer->cur_pic->i_frame) {
    picBuffer->cur_pic->p_frame = false;
    picBuffer->cur_pic->ipf = true;
    picBuffer->cur_pic->grf = true;
    picBuffer->cur_pic->arf = true;
    for (i = 0; i < BUFFER_SIZE + 1; i++) {
      if (&picBuffer->refPic[i] != picBuffer->cur_pic) {
        ResetRefPic(&picBuffer->refPic[i]);
      }
    }
  }

  /* Initialize reference picture list, note that API (user) can change
   * reference picture list */
  for (i = 0; i < BUFFER_SIZE; i++) {
    ResetRefPic(&picBuffer->refPicList[i]);
  }
  RefPicListInitialization(picBuffer);
}

void UpdatePictureBuffer(picBuffer* picBuffer) {
  refPic * cur_pic,*tmp,*refPic,*refPicList;
  int32_t i, j;

  refPicList = picBuffer->refPicList; /* Reference frame list */
  refPic     = picBuffer->refPic;     /* Reference frame store */
  cur_pic    = picBuffer->cur_pic;    /* Reconstructed picture */
  picBuffer->last_pic = picBuffer->cur_pic;

  /* Reset old marks from reference frame store if user wants to change
   * current ips/grf/arf frames. */

  /* Input picture marking */
  for (i = 0; i < picBuffer->size + 1; i++) {
    if (&refPic[i] == cur_pic) continue;
    if (cur_pic->ipf) refPic[i].ipf = false;
    if (cur_pic->grf) refPic[i].grf = false;
    if (cur_pic->arf) refPic[i].arf = false;
  }

  /* Reference picture marking */
  for (i = 0; i < picBuffer->size; i++) {
    for (j = 0; j < picBuffer->size + 1; j++) {
      if (refPicList[i].grf) refPic[j].grf = false;
      if (refPicList[i].arf) refPic[j].arf = false;
    }
  }

  /* Reference picture status is changed */
  for (i = 0; i < picBuffer->size; i++) {
    if (refPicList[i].grf) refPicList[i].refPic->grf = true;
    if (refPicList[i].arf) refPicList[i].refPic->arf = true;
  }

  /* Find new picture not used as reference and set it to new cur_pic */
  for (i = 0; i < picBuffer->size + 1; i++) {
    tmp = &refPic[i];
    if (!tmp->ipf && !tmp->arf && !tmp->grf) {
      picBuffer->cur_pic = &refPic[i];
      break;
    }
  }
}

void RefPicListInitialization(picBuffer* picBuffer) {
  refPic * cur_pic,*refPic,*refPicList;
  int32_t i, j = 0;

  refPicList = picBuffer->refPicList; /* Reference frame list */
  refPic     = picBuffer->refPic;     /* Reference frame store */
  cur_pic    = picBuffer->cur_pic;    /* Reconstructed picture */

  /* The first in the list is immediately previous picture. Note that
   * cur_pic (the picture under reconstruction) is skipped */
  for (i = 0; i < picBuffer->size + 1; i++) {
    if (refPic[i].ipf && (&refPic[i] != cur_pic)) {
      refPicList[j++] = refPic[i];
      break;
    }
  }

  /* The second in the list is golden frame */
  for (i = 0; i < picBuffer->size + 1; i++) {
    if (refPic[i].grf && (&refPic[i] != cur_pic)) {
      refPicList[j++] = refPic[i];
      break;
    }
  }

  /* The third in the list is alternative reference frame */
  for (i = 0; i < picBuffer->size + 1; i++) {
    if (refPic[i].arf && (&refPic[i] != cur_pic)) {
      refPicList[j] = refPic[i];
      break;
    }
  }

  /* Reset the ipf/grf/arf flags */
  for (i = 0; i < picBuffer->size; i++) {
    refPicList[i].ipf = false;
    refPicList[i].grf = false;
    refPicList[i].arf = false;
  }
}

void ResetRefPic(refPic* refPic) {
  refPic->poc = -1;
  refPic->i_frame = false;
  refPic->p_frame = false;
  refPic->show = false;
  refPic->ipf = false;
  refPic->arf = false;
  refPic->grf = false;
  refPic->search = false;
}

/*------------------------------------------------------------------------------
    PictureBufferSetRef

        Set the ASIC reference and reconstructed frame buffers based
        on the user preference and picture buffer.
------------------------------------------------------------------------------*/
void PictureBufferSetRef(picBuffer* picBuffer, asicData_s* asic) {
  int32_t i, refIdx = -1, refIdx2 = -1;
  refPic* refPicList = picBuffer->refPicList;
  int32_t noGrf = 0, noArf = 0;

  /* Amount of buffered frames limits grf/arf availability. */
  if (picBuffer->size < 2) {noGrf = 1;
    picBuffer->cur_pic->grf = false; }
  if (picBuffer->size < 3) {noArf = 1;
    picBuffer->cur_pic->arf = false; }

  /* If current picture shall refresh grf/arf remove marks from ref list */
  for (i = 0; i < picBuffer->size; i++) {
    if (picBuffer->cur_pic->grf || noGrf)
      picBuffer->refPicList[i].grf = false;
    if (picBuffer->cur_pic->arf || noArf)
      picBuffer->refPicList[i].arf = false;
  }

  /* ASIC can use one or two reference frame, use the first ones marked. */
  for (i = 0; i < BUFFER_SIZE; i++) {
    if ((i < picBuffer->size) && refPicList[i].search) {
      if (refIdx == -1)
        refIdx = i;
      else if (refIdx2 == -1)
        refIdx2 = i;
      else
        refPicList[i].search = 0;
    } else {
      refPicList[i].search = 0;
    }
  }

  /* If no reference specified, use ipf */
  if (refIdx == -1)
    refIdx = 0;

  asic->regs.mvRefIdx[0] = asic->regs.mvRefIdx[1] = refIdx;

  /* Set the reference buffer for ASIC, no reference for intra frames */
  if (picBuffer->cur_pic->p_frame) {
    /* Mark the ref pic that is used */
    picBuffer->refPicList[refIdx].search = 1;

    /* Check that enough frame buffers is available. */
    ASSERT(refPicList[refIdx].picture.lum);

    asic->regs.internalImageLumBaseR[0] = refPicList[refIdx].picture.lum;
    asic->regs.internalImageChrBaseR[0] = refPicList[refIdx].picture.cb;
    asic->regs.internalImageLumBaseR[1] = refPicList[refIdx].picture.lum;
    asic->regs.internalImageChrBaseR[1] = refPicList[refIdx].picture.cb;

    asic->regs.mvRefIdx[0] = asic->regs.mvRefIdx[1] = refIdx;
    asic->regs.ref2Enable = 0;

    /* Enable second reference frame usage */
    if (refIdx2 != -1) {
      asic->regs.internalImageLumBaseR[1] = refPicList[refIdx2].picture.lum;
      asic->regs.internalImageChrBaseR[1] = refPicList[refIdx2].picture.cb;
      asic->regs.mvRefIdx[1] = refIdx2;
      asic->regs.ref2Enable = 1;
    }
  }

  /* Set the reconstructed frame buffer for ASIC. Luma can be written
   * to same buffer but chroma read and write buffers must be different. */
  asic->regs.recWriteDisable = 0;
  if (!picBuffer->cur_pic->picture.lum) {
    refPic* cur_pic = picBuffer->cur_pic;
    refPic* cand;
    int32_t recIdx = -1;

    /* No free luma buffer so we must "steal" a luma buffer from
     * some other ref pic that is no longer needed. */
    for (i = 0; i < picBuffer->size + 1; i++) {
      cand = &picBuffer->refPic[i];
      if (cand == cur_pic) continue;
      if (((cur_pic->ipf | cand->ipf) == cur_pic->ipf) &&
          ((cur_pic->grf | cand->grf) == cur_pic->grf) &&
          ((cur_pic->arf | cand->arf) == cur_pic->arf))
        recIdx = i;
    }

    if (recIdx >= 0) {
      /* recIdx is overwritten or unused so steal it */
      cur_pic->picture.lum = picBuffer->refPic[recIdx].picture.lum;
      picBuffer->refPic[recIdx].picture.lum = 0;
    } else {
      /* No available buffer found, must be no refresh */
      ASSERT((cur_pic->ipf | cur_pic->grf | cur_pic->arf) == 0);
      asic->regs.recWriteDisable = 1;
    }
  }
  asic->regs.internalImageLumBaseW = picBuffer->cur_pic->picture.lum;
  asic->regs.internalImageChrBaseW = picBuffer->cur_pic->picture.cb;
}

