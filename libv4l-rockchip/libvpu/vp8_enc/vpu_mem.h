/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __VPU_MEM_H__
#define __VPU_MEM_H__

#include <stdint.h>

typedef struct VPUMem {
  uint32_t  phy_addr;
  uint32_t* vir_addr;
  uint32_t  size;
} VPUMemLinear_t;

int32_t VPUMallocLinear(VPUMemLinear_t* p, uint32_t size);
void VPUFreeLinear(VPUMemLinear_t* p);

#endif /* __VPU_MEM_H__ */

