/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "vpu_mem.h"

#include <malloc.h>
#include <memory.h>

#include "libvpu/rk_vepu_debug.h"

int32_t VPUMallocLinear(VPUMemLinear_t* p, uint32_t size) {
  p->vir_addr = (uint32_t*) calloc(1, size);
  if (p->vir_addr == NULL) {
    VPU_PLG_ERR("Fail to malloc.");
    return -1;
  }
  p->size = size;
  p->phy_addr = 0x0;
  return 0;
}

void VPUFreeLinear(VPUMemLinear_t* p) {
  free(p->vir_addr);
  memset(p, 0, sizeof(VPUMemLinear_t));
}
