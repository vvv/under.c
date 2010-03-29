#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#ifndef MAX
#  define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifdef DEBUG
#  define debug_print(format, ...)			\
	do {						\
		fputs("*DEBUG* ", stderr);		\
		fprintf(stderr, format, ##__VA_ARGS__); \
		fputc('\n', stderr);			\
	} while (0)
#else
#  define debug_print(format, ...)
#endif

#define die(errnum, format, ...) \
	error(1, errnum, format, ##__VA_ARGS__)

#ifdef DEBUG
#  define UNDEFINED \
	error_at_line(1, 0, __FILE__, __LINE__, "XXX not implemented")
#endif

/*
 * A stream is a (continuing) sequence of elements bundled in Chunks.
 *
 * data Stream el = EOF (Maybe ErrMsg) | Chunk [el]
 */
struct Stream {
	enum {
		S_EOF,  /* stream is exhausted (due to EOF or some error) */
		S_CHUNK /* stream is not terminated yet */
	} type;

	/*
	 * If `type' is S_EOF, `dat' is either NULL (EOF reached
	 * without error) or points to null-terminated error message.
	 *
	 * Otherwise (S_CHUNK), `dat' points to chunk data.
	 */
	const unsigned char *data;

	/*
	 * Length of data chunk (S_CHUNK only).
	 *
	 * Zero length signifies a stream with no currently available
	 * data but which is still continuing. A stream processor
	 * should, informally speaking, ``suspend itself'' and wait
	 * for more data to arrive.
	 *
	 * This value is meaningless for S_EOF streams.
	 */
	size_t size;
};

/* Memory buffer. */
struct MemBuf {
	unsigned char *buf; /* pointer to malloc-ated memory buffer */
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
		die(errno, "Virtual memory exhausted");
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
		debug_print("maybe_realloc: realloc(%p, %d)",
			    mem->buf, mem->bufsize);
		mem->buf = xrealloc(mem->buf, mem->bufsize);
	}

	return 0;
}

static void
_shift(size_t n, struct Stream *str, size_t *filepos)
{
	assert(n <= str->size);

	str->data += n;
	str->size -= n;

	*filepos += n;
}

static int
_parse_tag_id(struct Stream *str, size_t *filepos)
{
	if (str->type == S_EOF)
		return (str->data == NULL) ? -1 : -2;

	if (str->size == 0)
		return -1;
	unsigned char c = *str->data;
	_shift(1, str, filepos);

	putchar('(');

	/* Tag class: Universal | Application | Context-specific | Private */
	putchar("uacp"[(c & 0xc0) >> 6]);

	if ((c & 0x1f) == 0x1f)
		UNDEFINED; /* tag number > 30 */
	else
		printf("%d ", c & 0x1f);

	puts(c & 0x20 ? "<XXX_constructed>)" : "<XXX_primitive>)");

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

	struct Stream str = { S_EOF, NULL, 0 };
	size_t filepos = 0;
	size_t nread = 0;

	while (1) {
		nread = fread(mem->buf, 1, mem->bufsize, f);
		if (nread == 0) {
			if (feof(f))
				break; /* done with file */
			else if (ferror(f))
				/* can't guarantee sane output any more */
				die(errno, inpath);
			else
				error_at_line(1, errno, __FILE__, __LINE__,
					      "impossible happened");
		}
		debug_print("%d bytes read", nread);

		str.type = S_CHUNK;
		str.data = mem->buf;
		str.size = nread;

		switch (_parse_tag_id(&str, &filepos)) {
		case -2:
			die(0, "%s:%d: %s", inpath, filepos, str.data);

		case -1:
			UNDEFINED;

		default:
			debug_print("_parse_tag_id succeeded");
			break;
		}
	}

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
