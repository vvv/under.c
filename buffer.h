/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _BUFFER_H
#define _BUFFER_H

#include <stdint.h>
#include <stddef.h>

/* Memory buffer abstraction */
struct Buffer {
	uint8_t *wptr; /* Pointer to unfilled data region */
	size_t size; /* Remaining capacity */

	size_t _max_size; /* Overall capacity */
};

#define BUFFER(name) struct Buffer name = { NULL, 0, 0 };

static inline void INIT_BUFFER(struct Buffer *buf)
{
	buf->wptr = NULL;
	buf->size = buf->_max_size = 0;
}

/*
 * Resize the buffer, allocating necessary amount dynamic memory.
 *
 * Return -1 if memory allocation failed, otherwise return 0.
 */
int buffer_resize(struct Buffer *buf, size_t size);

/* Start of data region */
static inline uint8_t * buffer_data(const struct Buffer *buf)
{
	return buf->wptr + buf->size - buf->_max_size;
}

/* Reset the buffer, making it empty */
static inline void buffer_reset(struct Buffer *buf)
{
	buf->wptr = buffer_data(buf);
	buf->size = buf->_max_size;
}

/* Number of stored bytes */
static inline size_t buffer_len(const struct Buffer *buf)
{
	return buf->_max_size - buf->size;
}

/*
 * Append memory area to the buffer.
 *
 * @dest: destination buffer
 * @src: start of memory area to copy from
 * @n: number of bytes to copy
 *
 * Return -1 if there is not enough space in the buffer, otherwise
 * return 0.
 */
int buffer_put(struct Buffer *dest, const void *src, size_t n);

/* Append the byte to the buffer */
int buffer_putc(struct Buffer *dest, uint8_t c);

/* Append formatted string to the buffer */
int buffer_printf(struct Buffer *dest, const char *format, ...);

/* Print over -- overwrite buffer contents with formatted string */
#define buffer_xprintf(dest, format, ...) \
	({ buffer_reset((dest));          \
		buffer_printf((dest), (format), ##__VA_ARGS__); })

#endif /* _BUFFER_H */
