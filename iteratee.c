/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdarg.h>

#include "iteratee.h"

void
set_error(struct Stream *str, const char *format, ...)
{
	if (str->errmsg != NULL)
		return;

	va_list ap;
	va_start(ap, format);

	xasprintf(&str->errmsg, format, ap);

	va_end(ap);
}

IterV
head(uint8_t *c, struct Stream *str)
{
	if (str->type == S_EOF) {
		set_error(str, "head: EOF");
		return IE_CONT;
	}

	if (str->size == 0)
		return IE_CONT;

	*c = *str->data;
	++str->data;
	--str->size;

	return IE_DONE;
}

IterV
drop_while(bool (*p)(uint8_t c), struct Stream *str)
{
	for (; str->size > 0; ++str->data, --str->size) {
		if (!p(*str->data))
			return IE_DONE;
	}
	return IE_CONT;
}
