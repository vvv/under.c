#ifndef _UTIL_H
#define _UTIL_H

#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define die(format, ...)  error(1, 0, format, ##__VA_ARGS__)
#define die_errno(format, ...)  error(1, errno, format, ##__VA_ARGS__)

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

/* Pascal string. */
struct Pstring {
	size_t size; /* size of buffer */
	char *data; /* pointer to malloc(3)-ated memory */
};
#define PSTRING_INIT { 0, NULL }

#endif /* _UTIL_H */
