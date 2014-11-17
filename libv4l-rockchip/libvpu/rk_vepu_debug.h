/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBVPU_RK_VEPU_DEBUG_H_
#define LIBVPU_RK_VEPU_DEBUG_H_

#include <stdio.h>

/*
 * The current log level. Higher value will enable more logs.
 */
extern int g_log_level;

#define VPU_PLG_DBG(fmt, args...) ((g_log_level >= 2) ?				\
	(void) fprintf(stderr, "%s:%d: " fmt, __func__, __LINE__, ## args)	\
		: (void) 0)

#define VPU_PLG_INF(fmt, args...) ((g_log_level >= 1) ?				\
	(void) fprintf(stderr, "%s:%d: " fmt, __func__, __LINE__, ## args)	\
		: (void) 0)

#define VPU_PLG_ERR(fmt, args...) ((g_log_level >= 0) ?				\
	(void) fprintf(stderr, "ERR, %s:%d: " fmt, __func__, __LINE__, ## args)	\
		: (void) 0)

#endif
