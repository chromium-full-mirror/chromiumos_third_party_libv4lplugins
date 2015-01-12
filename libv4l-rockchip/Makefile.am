# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if WITH_V4L_PLUGINS
libv4l2plugin_LTLIBRARIES = libv4l-encplugin.la
endif

libv4l_encplugin_la_SOURCES = \
	libv4l-encplugin-rockchip.c \
	libvpu/rk_vepu.c \
	libvpu/rk_vepu_debug.c \
	libvpu/vp8_enc/encasiccontroller.c \
	libvpu/vp8_enc/encasiccontroller_v2.c \
	libvpu/vp8_enc/rk_vp8encapi.c \
	libvpu/vp8_enc/vp8codeframe.c \
	libvpu/vp8_enc/vp8encapi.c \
	libvpu/vp8_enc/vp8entropy.c \
	libvpu/vp8_enc/vp8header.c \
	libvpu/vp8_enc/vp8init.c \
	libvpu/vp8_enc/vp8macroblocktools.c \
	libvpu/vp8_enc/vp8picparameterset.c \
	libvpu/vp8_enc/vp8picturebuffer.c \
	libvpu/vp8_enc/vp8putbits.c \
	libvpu/vp8_enc/vp8ratecontrol.c \
	libvpu/vp8_enc/vpu_mem.c
libv4l_encplugin_la_CPPFLAGS = $(CFLAG_VISIBILITY)
libv4l_encplugin_la_LDFLAGS = -avoid-version -module -shared -export-dynamic
libv4l_encplugin_la_LIBADD = -lpthread
