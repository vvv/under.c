/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "buffer.h"

int
buffer_resize(struct Buffer *buf, size_t size)
{
	uint8_t *p = realloc(buffer_data(buf), size + 1);
	if (p == NULL)
		return -1;
	p[size] = 0; /* hidden null byte ('\0') */

	buf->wptr = p;
	buf->_max_size = buf->size = size;
	return 0;
}

int
buffer_put(struct Buffer *dest, const void *src, size_t n)
{
	if (dest->size < n)
		return -1;

	memcpy(dest->wptr, src, n);
	dest->wptr += n;
	dest->size -= n;

	return 0;
}

int
buffer_putc(struct Buffer *dest, uint8_t c)
{
	if (dest->size == 0)
		return -1;

	*dest->wptr = c;
	++dest->wptr;
	--dest->size;

	return 0;
}

int
buffer_printf(struct Buffer *dest, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	size_t n = vsnprintf((char *) dest->wptr, dest->size, format, ap);
	va_end(ap);

	if (n < dest->size) {
		dest->wptr += ++n; /* position after the trailing '\0' */
		dest->size -= n;
		return 0;
	}

	dest->wptr += dest->size;
	dest->size = 0;
	return -1;
}
