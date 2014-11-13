/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>
#include <sys/syscall.h>
#include "libv4l-plugin.h"
#include "libvpu/rk_vepu_interface.h"

#define SYS_IOCTL(fd, cmd, arg) \
	syscall(SYS_ioctl, (int)(fd), (unsigned long)(cmd), (void *)(arg))
#define SYS_READ(fd, buf, len) \
	syscall(SYS_read, (int)(fd), (void *)(buf), (size_t)(len));
#define SYS_WRITE(fd, buf, len) \
	syscall(SYS_write, (int)(fd), (const void *)(buf), (size_t)(len));

#if HAVE_VISIBILITY
#define PLUGIN_PUBLIC __attribute__ ((__visibility__("default")))
#else
#define PLUGIN_PUBLIC
#endif

static void *plugin_init(int fd)
{
	return NULL;
}

static void plugin_close(void *dev_ops_priv)
{
}

static int plugin_ioctl(void *dev_ops_priv, int fd,
			unsigned long int cmd, void *arg)
{
	return SYS_IOCTL(fd, cmd, arg);
}

PLUGIN_PUBLIC const struct libv4l_dev_ops libv4l2_plugin = {
	.init = &plugin_init,
	.close = &plugin_close,
	.ioctl = &plugin_ioctl,
};
