/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/syscall.h>
#include "config.h"  /* For HAVE_VISIBILITY */
#include "libv4l-plugin.h"
#include "libvpu/rk_vepu_debug.h"
#include "libvpu/rk_vepu_interface.h"

#define VLOG(log_level, str, ...) ((g_log_level >= log_level) ?			\
	(void) fprintf(stderr, "%s: " str "\n", __func__, ##__VA_ARGS__)	\
		: (void) 0)

#define VLOG_FD(log_level, str, ...) ((g_log_level >= log_level) ?		\
	(void) fprintf(stderr,							\
		"%s: fd=%d. " str "\n", __func__, fd, ##__VA_ARGS__) : (void) 0)

#define SYS_IOCTL(fd, cmd, arg) ({ 						\
	int ret = syscall(SYS_ioctl, (int)(fd), (unsigned long)(cmd),		\
			(void *)(arg));						\
	if ((ret && errno != EAGAIN) || g_log_level >= 2)			\
		fprintf(stderr, "SYS_ioctl: %s(%lu): fd=%d, ret=%d, errno=%d\n",\
			v4l_cmd2str(cmd), _IOC_NR((unsigned long)cmd), fd, ret,	\
			errno);							\
	ret;									\
	})

#if HAVE_VISIBILITY
#define PLUGIN_PUBLIC __attribute__ ((__visibility__("default")))
#else
#define PLUGIN_PUBLIC
#endif

#define RK3288_VPU_NAME "rk3288-vpu-enc"
#define DEFAULT_FRAME_RATE 30
#define DEFAULT_BITRATE 1000000
#define PENDING_BUFFER_QUEUE_SIZE VIDEO_MAX_FRAME

/*
 * struct pending_buffer - A v4l2 buffer pending for QBUF.
 * @buffer:	v4l2 buffer for QBUF.
 * @planes:	plane info of v4l2 buffer.
 * @next_runtime_param:	runtime parameters like framerate, bitrate, and
 * 			keyframe for the next buffer.
 */
struct pending_buffer {
	struct v4l2_buffer buffer;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct rk_vepu_runtime_param next_runtime_param;
};

/*
 * struct pending_buffer_queue - a ring buffer of pending buffers.
 * @count: the number of buffers stored in the array.
 * @front: the index of the first ready buffer.
 * @buf_array: pending buffer array.
 */
struct pending_buffer_queue {
	uint32_t count;
	int32_t front;
	struct pending_buffer buf_array[PENDING_BUFFER_QUEUE_SIZE];
};

/*
 * struct encoder_context - the context of an encoder instance.
 * @enc:	Encoder instance returned from rk_vepu_create().
 * @mutex:	The mutex to protect encoder_context.
 * @output_streamon_type:	Type of output interface when it streams on.
 * @capture_streamon_type:	Type of capture interface when it streams on.
 * @init_param:	Encoding parameters like input format, resolution, and etc.
 * 		These parameters will be passed to encoder at libvpu
 * 		initialization.
 * @runtime_param:	Runtime parameters like framerate, bitrate, and
 * 			keyframe. This is only used for receiving ext_ctrls
 * 			before streamon and pending buffer queue is empty.
 * @pending_buffers:	The pending v4l2 buffers waiting for the encoding
 *			configuration. After a previous buffer is dequeued,
 *			one buffer from the queue can be queued.
 * @can_qbuf:	Indicate that we can queue one source buffer. This is true only
 *		when the parameters to pass together with the source buffer are
 *		ready; those params are received on dequeing the previous
 *		destination buffer.
 * @get_param_payload:	Payload of V4L2_CID_PRIVATE_RK3288_GET_PARAMS. This is
 *			used to update the encoder configuration by
 *			rk_vepu_update_config().
 * @get_param_payload_size:	The size of get_param_payload.
 * @v4l2_ctrls:	v4l2 controls for VIDIOC_S_EXT_CTRLS.
 */
struct encoder_context {
	void *enc;
	pthread_mutex_t mutex;
	enum v4l2_buf_type output_streamon_type;
	enum v4l2_buf_type capture_streamon_type;
	struct rk_vepu_init_param init_param;
	struct rk_vepu_runtime_param runtime_param;
	struct pending_buffer_queue pending_buffers;
	bool can_qbuf;
	void *get_param_payload;
	size_t get_param_payload_size;
	struct v4l2_ext_control v4l2_ctrls[MAX_NUM_GET_CONFIG_CTRLS];
};

static void *plugin_init(int fd);
static void plugin_close(void *dev_ops_priv);
static int plugin_ioctl(void *dev_ops_priv, int fd, unsigned long int cmd,
		void *arg);

/* Functions to handle various ioctl. */
static int ioctl_streamon_locked(
	struct encoder_context *ctx, int fd, enum v4l2_buf_type *type);
static int ioctl_streamoff_locked(
	struct encoder_context *ctx, int fd, enum v4l2_buf_type *type);
static int ioctl_qbuf_locked(struct encoder_context *ctx, int fd,
	struct v4l2_buffer *buffer);
static int ioctl_dqbuf_locked(struct encoder_context *ctx, int fd,
	struct v4l2_buffer *buffer);
static int ioctl_s_ext_ctrls_locked(struct encoder_context *ctx, int fd,
	struct v4l2_ext_controls *ext_ctrls);
static int ioctl_s_parm_locked(struct encoder_context *ctx, int fd,
	struct v4l2_streamparm *parms);
static int ioctl_reqbufs_locked(struct encoder_context *ctx, int fd,
	struct v4l2_requestbuffers *reqbufs);

/* Helper functions to manipulate the pending buffer queue. */

static void queue_init(struct pending_buffer_queue *queue);
static bool queue_empty(struct pending_buffer_queue *queue);
static bool queue_full(struct pending_buffer_queue *queue);
/* Insert a buffer to the tail of the queue. */
static int queue_push_back(struct pending_buffer_queue *queue,
	struct v4l2_buffer *buffer);
/* Remove a buffer from the head of the queue. */
static void queue_pop_front(struct pending_buffer_queue *queue);
static struct pending_buffer *queue_front(struct pending_buffer_queue *queue);
static struct pending_buffer *queue_back(struct pending_buffer_queue *queue);

/* Returns true if the fd is Rockchip encoder device. */
bool is_rockchip_encoder(int fd);
/* Set encoder configuration to the driver. */
int set_encoder_config_locked(struct encoder_context *ctx, int fd,
	uint32_t buffer_index, size_t num_ctrls, uint32_t ctrls_ids[],
	void **payloads, uint32_t payload_sizes[]);
/* QBUF a buffer from the pending buffer queue if it is not empty. */
static int qbuf_if_pending_buffer_exists_locked(struct encoder_context *ctx,
	int fd);
/* Get the encoder parameters using G_FMT and initialize libvpu. */
static int initialize_libvpu(struct encoder_context *ctx, int fd);
/* Return the string represenation of a libv4l command for debugging. */
static const char *v4l_cmd2str(unsigned long int cmd);
/* Get the log level from the environment variable LIBV4L_PLUGIN_LOG_LEVEL. */
static void get_log_level();
static pthread_once_t g_get_log_level_once = PTHREAD_ONCE_INIT;

static void *plugin_init(int fd)
{
	int ret;
	struct v4l2_query_ext_ctrl ext_ctrl;

	pthread_once(&g_get_log_level_once, get_log_level);

	VLOG_FD(1, "");
	if (!is_rockchip_encoder(fd))
		return NULL;

	struct encoder_context *ctx = (struct encoder_context *)
		calloc(1, sizeof(struct encoder_context));
	if (ctx == NULL) {
		errno = ENOMEM;
		goto fail;
	}
	ret = pthread_mutex_init(&ctx->mutex, NULL);
	if (ret)
		goto fail;
	queue_init(&ctx->pending_buffers);

	memset(&ext_ctrl, 0, sizeof(ext_ctrl));
	ext_ctrl.id = V4L2_CID_PRIVATE_RK3288_GET_PARAMS;
	ret = SYS_IOCTL(fd, VIDIOC_QUERY_EXT_CTRL, &ext_ctrl);
	if (ret) {
		goto fail;
	}
	ctx->get_param_payload_size = ext_ctrl.elem_size;
	ctx->get_param_payload = calloc(1, ctx->get_param_payload_size);
	if (ctx->get_param_payload == NULL) {
		errno = ENOMEM;
		goto fail;
	}
	ctx->runtime_param.framerate_numer = DEFAULT_FRAME_RATE;
	ctx->runtime_param.framerate_denom = 1;
	ctx->runtime_param.bitrate = DEFAULT_BITRATE;
	VLOG_FD(1, "Success. ctx=%p", ctx);
	return ctx;

fail:
	plugin_close(ctx);
	return NULL;
}

static void plugin_close(void *dev_ops_priv)
{
	struct encoder_context *ctx = (struct encoder_context *)dev_ops_priv;

	VLOG(1, "ctx=%p", ctx);
	if (ctx == NULL)
		return;

	pthread_mutex_lock(&ctx->mutex);
	if (ctx->enc)
		rk_vepu_deinit(ctx->enc);
	free(ctx->get_param_payload);
	ctx->get_param_payload = NULL;
	pthread_mutex_unlock(&ctx->mutex);
	pthread_mutex_destroy(&ctx->mutex);

	free(ctx);
}

static int plugin_ioctl(void *dev_ops_priv, int fd,
			unsigned long int cmd, void *arg)
{
	int ret;
	struct encoder_context *ctx = (struct encoder_context *)dev_ops_priv;

	VLOG_FD(1, "%s(%lu)", v4l_cmd2str(cmd), _IOC_NR(cmd));

	pthread_mutex_lock(&ctx->mutex);
	switch (cmd) {
	case VIDIOC_STREAMON:
		ret = ioctl_streamon_locked(ctx, fd, arg);
		break;

	case VIDIOC_STREAMOFF:
		ret = ioctl_streamoff_locked(ctx, fd, arg);
		break;

	case VIDIOC_QBUF:
		ret = ioctl_qbuf_locked(ctx, fd, arg);
		break;

	case VIDIOC_DQBUF:
		ret = ioctl_dqbuf_locked(ctx, fd, arg);
		break;

	case VIDIOC_S_EXT_CTRLS:
		ret = ioctl_s_ext_ctrls_locked(ctx, fd, arg);
		break;

	case VIDIOC_S_PARM:
		ret = ioctl_s_parm_locked(ctx, fd, arg);
		break;

	case VIDIOC_REQBUFS:
		ret = ioctl_reqbufs_locked(ctx, fd, arg);
		break;

	default:
		ret = SYS_IOCTL(fd, cmd, arg);
		break;
	}
	pthread_mutex_unlock(&ctx->mutex);
	return ret;
}

static int ioctl_streamon_locked(
		struct encoder_context *ctx, int fd, enum v4l2_buf_type *type)
{
	int ret = SYS_IOCTL(fd, VIDIOC_STREAMON, type);
	if (ret)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(*type))
		ctx->output_streamon_type = *type;
	else
		ctx->capture_streamon_type = *type;
	if (ctx->output_streamon_type && ctx->capture_streamon_type) {
		ret = initialize_libvpu(ctx, fd);
		if (ret)
			return ret;
		ctx->can_qbuf = true;
		return qbuf_if_pending_buffer_exists_locked(ctx, fd);
	}
	return 0;
}

static int ioctl_streamoff_locked(
		struct encoder_context *ctx, int fd, enum v4l2_buf_type *type)
{
	int ret = SYS_IOCTL(fd, VIDIOC_STREAMOFF, type);
	if (ret)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(*type))
		ctx->output_streamon_type = 0;
	else
		ctx->capture_streamon_type = 0;
	return 0;
}

static int ioctl_qbuf_locked(struct encoder_context *ctx, int fd,
		struct v4l2_buffer *buffer)
{
	size_t num_ctrls = 0;
	uint32_t *ctrl_ids = NULL, *payload_sizes = NULL;
	void **payloads = NULL;
	int ret;

	if (!V4L2_TYPE_IS_OUTPUT(buffer->type)) {
		return SYS_IOCTL(fd, VIDIOC_QBUF, buffer);
	}

	if (!ctx->can_qbuf) {
		VLOG_FD(1, "Put buffer (%d) in the pending queue.",
			buffer->index);
		/*
		 * The last frame is not encoded yet. Put the buffer to the
		 * pending queue.
		 */
		return queue_push_back(&ctx->pending_buffers, buffer);
	}
	/* Get the encoder configuration from the library. */
	if (rk_vepu_get_config(ctx->enc, &num_ctrls, &ctrl_ids, &payloads,
			&payload_sizes)) {
		VLOG_FD(0, "rk_vepu_get_config failed");
		return -EIO;
	}
	/* Set the encoder configuration to the driver. */
	ret = set_encoder_config_locked(ctx, fd, buffer->index, num_ctrls, ctrl_ids,
			payloads, payload_sizes);
	if (ret)
		return ret;

	buffer->config_store = buffer->index + 1;
	ret = SYS_IOCTL(fd, VIDIOC_QBUF, buffer);
	if (ret == 0)
		ctx->can_qbuf = false;
	else
		VLOG(0, "QBUF failed. errno=%d", errno);
	return ret;
}

static int ioctl_dqbuf_locked(struct encoder_context *ctx, int fd,
		struct v4l2_buffer *buffer)
{
	struct v4l2_ext_controls ext_ctrls;
	struct v4l2_ext_control v4l2_ctrl;
	int ret;
	uint32_t bytesused;

	if (V4L2_TYPE_IS_OUTPUT(buffer->type)) {
		return SYS_IOCTL(fd, VIDIOC_DQBUF, buffer);
	}

	ret = SYS_IOCTL(fd, VIDIOC_DQBUF, buffer);
	if (ret)
		return ret;

	assert(!ctx->can_qbuf);

	/* Get the encoder configuration and update the library. */
	memset(ctx->get_param_payload, 0, ctx->get_param_payload_size);
	memset(&v4l2_ctrl, 0, sizeof(v4l2_ctrl));
	v4l2_ctrl.id = V4L2_CID_PRIVATE_RK3288_GET_PARAMS;
	v4l2_ctrl.size = ctx->get_param_payload_size;
	v4l2_ctrl.ptr = ctx->get_param_payload;
	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	/* TODO: change this to config_store after the header is updated. */
	ext_ctrls.ctrl_class = 0;
	ext_ctrls.count = 1;
	ext_ctrls.controls = &v4l2_ctrl;
	ret = SYS_IOCTL(fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
	if (ret)
		return ret;
	bytesused = V4L2_TYPE_IS_MULTIPLANAR(buffer->type) ?
		buffer->m.planes[0].bytesused : buffer->bytesused;

	if (rk_vepu_update_config(ctx->enc, v4l2_ctrl.ptr, v4l2_ctrl.size,
			bytesused)) {
		VLOG_FD(0, "rk_vepu_update_config failed.");
		return -EIO;
	}
	ctx->can_qbuf = true;
	return qbuf_if_pending_buffer_exists_locked(ctx, fd);
}

static int ioctl_s_ext_ctrls_locked(struct encoder_context *ctx, int fd,
		struct v4l2_ext_controls *ext_ctrls)
{
	size_t i;
	struct rk_vepu_runtime_param *runtime_param_ptr;

	bool no_pending_buffer = queue_empty(&ctx->pending_buffers);
	/*
	 * If buffer queue is empty, update parameters directly.
	 * If buffer queue is not empty, save parameters to the last buffer. And
	 * these values will be sent again when the buffer is ready to deliver.
	 */
	if (!no_pending_buffer) {
		struct pending_buffer *element = queue_back(&ctx->pending_buffers);
		runtime_param_ptr = &element->next_runtime_param;
	} else {
		runtime_param_ptr = &ctx->runtime_param;
	}

	/*
	 * Check each extension control to update keyframe and bitrate
	 * parameters.
	 */
	for (i = 0; i < ext_ctrls->count; i++) {
		switch (ext_ctrls->controls[i].id) {
		case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
			runtime_param_ptr->keyframe_request = true;
			break;
		case V4L2_CID_MPEG_VIDEO_BITRATE:
			runtime_param_ptr->bitrate = ext_ctrls->controls[i].value;
			break;
		default:
			break;
		}
	}

	if (no_pending_buffer && ctx->enc) {
		if (rk_vepu_update_parameter(ctx->enc, runtime_param_ptr)) {
			VLOG_FD(0, "rk_vepu_update_parameter failed.");
			return -EIO;
		}
		memset(runtime_param_ptr, 0, sizeof(struct rk_vepu_runtime_param));
	}
	/* Driver should ignore keyframe and bitrate controls */
	return SYS_IOCTL(fd, VIDIOC_S_EXT_CTRLS, ext_ctrls);
}

static int ioctl_s_parm_locked(struct encoder_context *ctx, int fd,
		struct v4l2_streamparm *parms)
{
	if (V4L2_TYPE_IS_OUTPUT(parms->type)
			&& parms->parm.output.timeperframe.denominator) {
		struct rk_vepu_runtime_param *runtime_param_ptr;
		bool no_pending_buffer = queue_empty(&ctx->pending_buffers);
		struct pending_buffer *element = queue_back(&ctx->pending_buffers);

		runtime_param_ptr = no_pending_buffer ? &ctx->runtime_param :
			&element->next_runtime_param;
		runtime_param_ptr->framerate_numer =
			parms->parm.output.timeperframe.denominator;
		runtime_param_ptr->framerate_denom =
			parms->parm.output.timeperframe.numerator;

		if (!no_pending_buffer || !ctx->enc)
			return 0;
		if (rk_vepu_update_parameter(ctx->enc, runtime_param_ptr)) {
			VLOG_FD(0, "rk_vepu_update_parameter failed.");
			return -EIO;
		}
		memset(runtime_param_ptr, 0, sizeof(struct rk_vepu_runtime_param));
		return 0;
	}
	return SYS_IOCTL(fd, VIDIOC_S_PARM, parms);
}

static int ioctl_reqbufs_locked(struct encoder_context *ctx, int fd,
		struct v4l2_requestbuffers *reqbufs)
{
	int ret = SYS_IOCTL(fd, VIDIOC_REQBUFS, reqbufs);
	if (ret)
		return ret;
	queue_init(&ctx->pending_buffers);
	return 0;
}

bool is_rockchip_encoder(int fd) {
	struct v4l2_capability cap;
	memset(&cap, 0, sizeof(cap));
	int ret = SYS_IOCTL(fd, VIDIOC_QUERYCAP, &cap);
	if (ret)
		return false;
	return strcmp(RK3288_VPU_NAME, (const char *)cap.driver) == 0;
}

int set_encoder_config_locked(struct encoder_context *ctx, int fd,
		uint32_t buffer_index, size_t num_ctrls, uint32_t ctrl_ids[],
		void **payloads, uint32_t payload_sizes[])
{
	size_t i;
	struct v4l2_ext_controls ext_ctrls;

	if (num_ctrls <= 0)
		return 0;

	assert(num_ctrls <= MAX_NUM_GET_CONFIG_CTRLS);
	if (num_ctrls > MAX_NUM_GET_CONFIG_CTRLS) {
		VLOG_FD(0, "The number of controls exceeds limit.");
		return -EIO;
	}
	memset(&ext_ctrls, 0, sizeof(ext_ctrls));
	/* TODO: change this to config_store after the header is updated. */
	ext_ctrls.ctrl_class = buffer_index + 1;
	ext_ctrls.count = num_ctrls;
	ext_ctrls.controls = ctx->v4l2_ctrls;
	memset(ctx->v4l2_ctrls, 0, sizeof(ctx->v4l2_ctrls));
	for (i = 0; i < num_ctrls; ++i) {
		ctx->v4l2_ctrls[i].id = ctrl_ids[i];
		ctx->v4l2_ctrls[i].ptr = payloads[i];
		ctx->v4l2_ctrls[i].size = payload_sizes[i];
	}
	int ret = SYS_IOCTL(fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
	if (ret) {
		return ret;
	}
	return 0;
}

static int qbuf_if_pending_buffer_exists_locked(struct encoder_context *ctx,
		int fd)
{
	if (!queue_empty(&ctx->pending_buffers)) {
		int ret;
		struct pending_buffer *element = queue_front(&ctx->pending_buffers);
		VLOG_FD(1, "QBUF a buffer (%d) from the pending queue.",
			element->buffer.index);
		if (rk_vepu_update_parameter(ctx->enc, &element->next_runtime_param)) {
			VLOG_FD(0, "rk_vepu_update_parameter failed.");
			return -EIO;
		}
		memset(&element->next_runtime_param, 0,
			sizeof(element->next_runtime_param));
		ret = ioctl_qbuf_locked(ctx, fd, &element->buffer);
		if (ret)
			return ret;
		queue_pop_front(&ctx->pending_buffers);
	}
	return 0;
}

static int initialize_libvpu(struct encoder_context *ctx, int fd)
{
	struct rk_vepu_init_param init_param;
	memset(&init_param, 0, sizeof(init_param));

	/* Get the input format. */
	struct v4l2_format format;
	memset(&format, 0, sizeof(format));
	format.type = ctx->output_streamon_type;
	int ret = SYS_IOCTL(fd, VIDIOC_G_FMT, &format);
	if (ret)
		return ret;
	init_param.input_format = format.fmt.pix_mp.pixelformat;

	/* Get the output format. */
	memset(&format, 0, sizeof(format));
	format.type = ctx->capture_streamon_type;
	ret = SYS_IOCTL(fd, VIDIOC_G_FMT, &format);
	if (ret)
		return ret;
	init_param.output_format = format.fmt.pix_mp.pixelformat;

	/* Get the cropped size. */
	struct v4l2_crop crop;
	memset(&crop, 0, sizeof(crop));
	crop.type = ctx->output_streamon_type;
	ret = SYS_IOCTL(fd, VIDIOC_G_CROP, &crop);
	if (ret)
		return ret;
	init_param.width = crop.c.width;
	init_param.height = crop.c.height;

	/*
	 * If the encoder library has initialized and parameters have not
	 * changed, skip the initialization.
	 */
	if (ctx->enc && memcmp(&init_param, &ctx->init_param, sizeof(init_param))) {
		rk_vepu_deinit(ctx->enc);
		ctx->enc = NULL;
	}
	if (!ctx->enc) {
		memcpy(&ctx->init_param, &init_param, sizeof(init_param));
		ctx->enc = rk_vepu_init(&init_param);
		if (ctx->enc == NULL) {
			VLOG_FD(0, "Failed to initialize encoder library.");
			return -EIO;
		}
	}
	if (rk_vepu_update_parameter(ctx->enc, &ctx->runtime_param)) {
		VLOG_FD(0, "rk_vepu_update_parameter failed.");
		return -EIO;
	}
	memset(&ctx->runtime_param, 0, sizeof(struct rk_vepu_runtime_param));
	return 0;
}

static void queue_init(struct pending_buffer_queue *queue)
{
	memset(queue, 0, sizeof(struct pending_buffer_queue));
}

static bool queue_empty(struct pending_buffer_queue *queue)
{
	return queue->count == 0;
}

static bool queue_full(struct pending_buffer_queue *queue)
{
	return queue->count == PENDING_BUFFER_QUEUE_SIZE;
}

static int queue_push_back(struct pending_buffer_queue *queue,
		struct v4l2_buffer *buffer)
{
	if (queue_full(queue))
	  return -ENOMEM;
	int rear = (queue->front + queue->count) % PENDING_BUFFER_QUEUE_SIZE;
	queue->count++;
	struct pending_buffer *entry = &queue->buf_array[rear];
	memset(entry, 0, sizeof(struct pending_buffer));

	memcpy(&entry->buffer, buffer, sizeof(*buffer));
	if (V4L2_TYPE_IS_MULTIPLANAR(buffer->type)) {
		memset(entry->planes, 0,
			sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);
		memcpy(entry->planes, buffer->m.planes,
			sizeof(struct v4l2_plane) * buffer->length);
		entry->buffer.m.planes = entry->planes;
	}
	return 0;
}

static void queue_pop_front(struct pending_buffer_queue *queue)
{
	assert(!queue_empty(queue));
	queue->count--;
	queue->front = (queue->front + 1) % PENDING_BUFFER_QUEUE_SIZE;
}

static struct pending_buffer *queue_front(struct pending_buffer_queue *queue)
{
	if (queue_empty(queue))
		return NULL;
	return &queue->buf_array[queue->front];
}

static struct pending_buffer *queue_back(struct pending_buffer_queue *queue)
{
	if (queue_empty(queue))
		return NULL;
	return &queue->buf_array[(queue->front + queue->count - 1) %
		PENDING_BUFFER_QUEUE_SIZE];
}

static void get_log_level()
{
	char *log_level_str = getenv("LIBV4L_PLUGIN_LOG_LEVEL");
	if (log_level_str != NULL)
		g_log_level = strtol(log_level_str, NULL, 10);
}

static const char* v4l_cmd2str(unsigned long int cmd)
{
	switch (cmd) {
	case VIDIOC_QUERYCAP:
		return "VIDIOC_QUERYCAP";
	case VIDIOC_TRY_FMT:
		return "VIDIOC_TRY_FMT";
	case VIDIOC_S_FMT:
		return "VIDIOC_S_FMT";
	case VIDIOC_G_FMT:
		return "VIDIOC_G_FMT";
	case VIDIOC_ENUM_FMT:
		return "VIDIOC_ENUM_FMT";
	case VIDIOC_S_PARM:
		return "VIDIOC_S_PARM";
	case VIDIOC_G_PARM:
		return "VIDIOC_G_PARM";
	case VIDIOC_QBUF:
		return "VIDIOC_QBUF";
	case VIDIOC_DQBUF:
		return "VIDIOC_DQBUF";
	case VIDIOC_PREPARE_BUF:
		return "VIDIOC_PREPARE_BUF";
	case VIDIOC_CREATE_BUFS:
		return "VIDIOC_CREATE_BUFS";
	case VIDIOC_REQBUFS:
		return "VIDIOC_REQBUFS";
	case VIDIOC_STREAMON:
		return "VIDIOC_STREAMON";
	case VIDIOC_STREAMOFF:
		return "VIDIOC_STREAMOFF";
	case VIDIOC_S_CROP:
		return "VIDIOC_S_CROP";
	case VIDIOC_S_CTRL:
		return "VIDIOC_S_CTRL";
	case VIDIOC_G_EXT_CTRLS:
		return "VIDIOC_G_EXT_CTRLS";
	case VIDIOC_S_EXT_CTRLS:
		return "VIDIOC_S_EXT_CTRLS";
	case VIDIOC_QUERYBUF:
		return "VIDIOC_QUERYBUF";
	default:
		return "UNKNOWN";
	}
}

PLUGIN_PUBLIC const struct libv4l_dev_ops libv4l2_plugin = {
	.init = &plugin_init,
	.close = &plugin_close,
	.ioctl = &plugin_ioctl,
};
