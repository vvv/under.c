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
	for (i = 1, p += 1; i < size; ++i, ++p)
		fprintf(stderr, " %02x", *p);

	fputs("]\n", stderr);
}
#endif
