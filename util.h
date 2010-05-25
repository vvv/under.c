#ifndef _UTIL_H
#define _UTIL_H

#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define die(format, ...)  error(1, 0, format, ##__VA_ARGS__)
#define die_errno(format, ...)  error(1, errno, format, ##__VA_ARGS__)

/* Source files that use `debug_print' need to #include <stdio.h> */
#ifdef DEBUG
#  define debug_print(format, ...) \
	fprintf(stderr, "(DEBUG) " format "\n", ##__VA_ARGS__)
#else
#  define debug_print(...)
#endif

#ifdef DEBUG
/*
 * Print hexadecimal dump of memory.
 *
 * @msg: prefix message (optional)
 * @addr: start of data
 * @size: number of bytes
 */
void debug_hexdump(const char *msg, void *addr, size_t size);
#else
#  define debug_hexdump(...)
#endif

#ifndef MAX
#  define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#  define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

static inline void *
xmalloc(size_t sz)
{
	void *rv = malloc(sz);
	if (rv == NULL)
		die("Out of memory, malloc failed");
	return rv;
}

static inline void *
xrealloc(void *ptr, size_t sz)
{
	void *rv = realloc(ptr, sz);
	if (rv == NULL)
		die("Out of memory, realloc failed");
	return rv;
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

/* Pascal string */
struct Pstring {
	size_t size;
	uint8_t *data;
};

#endif /* _UTIL_H */
