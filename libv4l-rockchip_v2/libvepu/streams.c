#include "streams.h"

#include <assert.h>
#include <memory.h>

/*
 * bit stream assembler
 */

void stream_buffer_init(struct stream_s *buffer)
{
	buffer->bits_cnt = 0;

	memset(buffer->buffer, 0, sizeof(buffer->buffer));
}

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define ROUND_UP(x, y) ((((x)-1) | __round_mask(x, y))+1)

#define STREAM_BYTES(stream) (stream->bits_cnt >> 3)
#define STREAM_LAST_BYTE(stream) (stream->buffer[STREAM_BYTES(stream)])
#define STREAM_TAILROOM(stream) \
	((sizeof(stream->buffer) << 3) - stream->bits_cnt)
#define STREAM_ALIGN_BITS(stream) (8 - (stream->bits_cnt % 8))

void stream_buffer_flush(struct stream_s *stream)
{
	stream->bits_cnt = ROUND_UP(stream->bits_cnt, 8);
}

int stream_buffer_bytes(struct stream_s *stream)
{
	return stream->bits_cnt >> 3;
}

void stream_put_bits(struct stream_s *stream, u32 value, int bits,
		     const char *name)
{
	if (bits > STREAM_TAILROOM(stream))
		return;

	while (bits > 0) {
		int align_bits = STREAM_ALIGN_BITS(stream);
		int n = align_bits > bits ? bits : align_bits;
		int shift = align_bits - bits;

		value &= ((1 << bits) - 1);
		STREAM_LAST_BYTE(stream)
			|= (u8)(shift > 0 ? value << shift : value >> -shift);

		bits -= n;
		stream->bits_cnt += n;
	}
}

void stream_write_ue(struct stream_s *fifo, u32 val, const char *name)
{
	u32 num_bits = 0;

	assert(val < 0x7fffffff);

	val++;
	while (val >> ++num_bits);

	if (num_bits > 12) {
		u32 tmp;

		tmp = num_bits - 1;

		if (tmp > 24) {
			tmp -= 24;
			stream_put_bits(fifo, 0, 24, name);
		}

		stream_put_bits(fifo, 0, tmp, name);

		if (num_bits > 24) {
			num_bits -= 24;
			stream_put_bits(fifo, val >> num_bits, 24, name);
			val &= (1 << num_bits) - 1;
		}

		stream_put_bits(fifo, val, num_bits, name);
	} else {
		stream_put_bits(fifo, val, 2 * num_bits - 1, name);
	}
}

void stream_write_se(struct stream_s *fifo, s32 val, const char *name)
{
	u32 tmp;

	if (val > 0)
		tmp = (u32)(2 * val - 1);
	else
		tmp = (u32)(-2 * val);

	stream_write_ue(fifo, tmp, name);
}

