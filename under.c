#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAX
#  define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

/* Memory buffer. */
struct MemBuf {
	char *buf;      /* pointer to malloc-ated memory buffer */
	size_t bufsize; /* size of buffer */
};

static inline int
streq(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}

static void *
xrealloc(void *ptr, size_t sz)
{
	void *rv = realloc(ptr, sz);
	if (rv == NULL)
		error(1, errno, "Virtual memory exhausted");
	return rv;
}

/*
 * Do nothing if block size of file `f' is equal to `mem->bufsize'
 * (this is the case when input files are taken from the same filesystem).
 * Otherwise (re)allocate memory buffer and update the fields of `mem'.
 */
static int
maybe_realloc(FILE *f, const char *fpath, struct MemBuf *mem)
{
	struct stat st;

	if (fstat(fileno(f), &st) < 0) {
		error(0, errno, fpath);
		return -1;
	}
	const unsigned long insize = st.st_blksize;

	if (insize != mem->bufsize) {
		if (fstat(fileno(stdout), &st) < 0) {
			error(0, errno, "fstat(STDOUT)");
			return -1;
		}
		const unsigned long outsize = st.st_blksize;

		mem->bufsize = MAX(insize, outsize);
#ifdef DEBUG
		fprintf(stderr, "*DEBUG* maybe_realloc: realloc(%p, %d)\n",
			mem->buf, mem->bufsize);
#endif
		mem->buf = xrealloc(mem->buf, mem->bufsize);
	}

	return 0;
}

static int
process(const char *inpath, struct MemBuf *mem)
{
	FILE *f = NULL;

	if (streq(inpath, "-")) {
		f = stdin;
	} else if ((f = fopen(inpath, "rb")) == NULL) {
		error(0, errno, inpath);
		return -1;
	}

	if (maybe_realloc(f, inpath, mem) < 0)
		return -1;

	/* XXX Here goes decoding stuff... */

	return streq(inpath, "-") ? 0 : fclose(f);
}

int
main(int argc, char **argv)
{
	struct MemBuf mem = { NULL, 0 };

	int rv = 0;
	if (argc == 1) {
		rv = process("-", &mem);
	} else {
		int i;
		for (i = 1; i < argc; i++)
			rv |= process(argv[i], &mem);
	}

	free(mem.buf);
	return -rv;
}
