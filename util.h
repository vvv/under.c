/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _UTIL_H
#define _UTIL_H

#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define die(format, ...)  error(1, 0, format, ##__VA_ARGS__)
#define die_errno(format, ...)  error(1, errno, format, ##__VA_ARGS__)

#ifdef DEBUG
/* Source files need to #include <stdio.h> in order to use `debug_print' */
#  define debug_print(format, ...) \
	fprintf(stderr, "(DEBUG) " format "\n", ##__VA_ARGS__)

/*
 * Print hexadecimal dump of memory.
 *
 * @msg: prefix message (optional)
 * @addr: start of data
 * @size: number of bytes
 */
void debug_hexdump(const char *msg, const void *addr, size_t size);
#else
#  define debug_print(...)
#  define debug_hexdump(...)
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifndef MAX
#  define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#  define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

static inline void *
xmalloc(size_t size)
{
	void *p = malloc(size);
	if (p == NULL)
		die("Out of memory, malloc failed");
	return p;
}

static inline void *
xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (p == NULL)
		die("Out of memory, realloc failed");
	return p;
}

/* Allocate memory for type and fill it with zero-valued bytes */
#define new_zeroed(type) ({		   \
	type *__x = xmalloc(sizeof(type)); \
	memset(__x, 0, sizeof(type));	   \
	__x; })

static inline int
streq(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}

/* Print to allocated string, die()ing on error */
void xasprintf(char **strp, const char *format, ...);

#endif /* _UTIL_H */
