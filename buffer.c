/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "util.h"

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
buffer_put(struct Buffer *dest, const void *src, size_t n, char **errmsg,
	   const char *buffer_name)
{
	if (dest->size < n) {
		if (buffer_name != NULL)
			set_error(errmsg, "Insufficient capacity of %s",
				  buffer_name);
		return -1;
	}

	memcpy(dest->wptr, src, n);
	dest->wptr += n;
	dest->size -= n;

	return 0;
}
