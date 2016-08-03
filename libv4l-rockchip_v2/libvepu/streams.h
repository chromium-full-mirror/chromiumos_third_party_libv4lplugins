#ifndef STREAMS_H
#define STREAMS_H

#include "common/rk_venc.h"

#define STREAM_BUFFER_SIZE 128

/* struct for assemble bitstream */
struct stream_s {
	u8 buffer[STREAM_BUFFER_SIZE];
	u32 bits_cnt;
};

void stream_buffer_init(struct stream_s *stream);
void stream_buffer_flush(struct stream_s *stream);
int stream_buffer_bytes(struct stream_s *stream);

void stream_put_bits(struct stream_s *buffer, u32 value, int bits,
		     const char *name);
void stream_write_se(struct stream_s *fifo, s32 val, const char *name);
void stream_write_ue(struct stream_s *fifo, u32 val, const char *name);

#endif
