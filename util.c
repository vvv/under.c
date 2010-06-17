/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include "util.h"

#ifdef DEBUG
void
debug_hexdump(const char *msg, void *addr, size_t size)
{
	fputs("(DEBUG) ", stderr);
	if (msg != NULL)
		fprintf(stderr, "%s ", msg);
	fputc('[', stderr);

	const uint8_t *p = addr;
	if (size != 0)
		fprintf(stderr, "%02x", *p);

	size_t i;
	for (i = 1, ++p; i < size; ++i, ++p)
		fprintf(stderr, " %02x", *p);

	fputs("]\n", stderr);
}
#endif
