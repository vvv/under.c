/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "util.h"

#ifdef DEBUG
void
debug_hexdump(const char *msg, const void *addr, size_t size)
{
	fputs("(DEBUG) ", stderr);
	if (msg != NULL)
		fprintf(stderr, "%s ", msg);
	fputc('[', stderr);

	if (size != 0) {
		const uint8_t *p = addr;
		fprintf(stderr, "%02x", *p);

		while(--size != 0)
			fprintf(stderr, " %02x", *(++p));
	}

	fputs("]\n", stderr);
}
#endif

void
xasprintf(char **strp, const char *format, ...)
{
	assert(*strp == NULL);

	va_list ap;
	va_start(ap, format);

	*strp = xmalloc(80);
	const size_t nchars = vsnprintf(*strp, 80, format, ap);
	if (nchars >= 80) {
		/* Not enough space.  Reallocate buffer .. */
		*strp = xrealloc(*strp, nchars + 1);

		 /* .. and try again. */
		vsnprintf(*strp, nchars + 1, format, ap);
	}

	va_end(ap);
}
