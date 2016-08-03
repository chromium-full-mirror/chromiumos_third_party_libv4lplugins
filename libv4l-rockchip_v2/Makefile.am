# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if WITH_V4L_PLUGINS
libv4l2plugin_LTLIBRARIES = libv4l-encplugin.la
endif

libv4l_encplugin_la_SOURCES = \
	libv4l-encplugin-rockchip.c \
	libvepu/streams.c \
	libvepu/rk_vepu.c \
	libvepu/rk_vepu_debug.c \
	libvepu/common/rk_venc_rate_control.c \
	libvepu/h264e/h264e.c \
	libvepu/h264e/h264e_rate_control.c \
	libvepu/vp8e/vp8e.c \
	libvepu/vp8e/boolhuff.c \
	libvepu/vp8e/vp8e_bitstream.c \
	libvepu/vp8e/vp8e_prob_adapt.c

libv4l_encplugin_la_CPPFLAGS = $(CFLAG_VISIBILITY)
libv4l_encplugin_la_LDFLAGS = -avoid-version -module -shared -export-dynamic
libv4l_encplugin_la_LIBADD = -lpthread
