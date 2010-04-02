#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>

#ifndef MAX
#  define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifdef DEBUG
#  define debug_print(format, ...) \
	fprintf(stderr, "(DEBUG) " format "\n", ##__VA_ARGS__)
#else
#  define debug_print(format, ...)
#endif

#define die_errno(format, ...)  error(1, errno, format, ##__VA_ARGS__)
#define die(format, ...)  error(1, 0, format, ##__VA_ARGS__)

#ifdef DEBUG
#  define UNDEFINED \
	error_at_line(1, 0, __FILE__, __LINE__, "XXX not implemented")
#endif

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
		die_errno("Virtual memory exhausted");
	return rv;
}

/* Memory buffer. */
struct MemBuf {
	unsigned char *data; /* pointer to malloc(3)-ated memory */
	size_t size; /* size of buffer */
};

/*
 * Adjust buffer to file's blocksize.
 *
 * Do nothing if size of buffer is equal to blocksize of file.
 * Otherwise (re)allocate memory.
 */
static int
adjust_buffer(FILE *f, struct MemBuf *buf)
{
	struct stat st;

	if (fstat(fileno(f), &st) < 0)
		return -1;
	const unsigned long insize = st.st_blksize;

	if (insize != buf->size) {
		if (fstat(fileno(stdout), &st) < 0) {
			error(0, errno, "fstat(STDOUT)");
			return -1;
		}
		const unsigned long outsize = st.st_blksize;

		buf->size = MAX(insize, outsize);
		debug_print("adjust_buffer: realloc(%p, %ld)", buf->data,
			    (unsigned long) buf->size);
		buf->data = xrealloc(buf->data, buf->size);
	}

	return 0;
}

/* ---------------------------------------------------------------------
 * Stream
 */

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
	 * without error) or points to a null-terminated error message.
	 *
	 * If `type' is S_CHUNK, `dat' points to chunk data, and
	 * `size' is equal to the number of bytes in chunk.
	 */
	const unsigned char *data;

	/*
	 * Size of chunk.
	 *
	 * S_CHUNK: Zero length signifies a stream with no currently
	 * available data but which is still continuing. A stream
	 * processor should, informally speaking, ``suspend itself''
	 * and wait for more data to arrive.
	 *
	 * `size' is always 0 for S_EOF, even if `data' points to an
	 * error message.
	 */
	size_t size;
};

static size_t
retrieve(struct Stream *str, FILE *input, unsigned char *buf, size_t bufsize)
{
	assert(str->type == S_CHUNK); /* XXX And what about S_EOF? */

	/* A chunk must be contained in `buf'. */
	assert(buf <= str->data);
	assert(str->data + str->size <= buf + bufsize);

	if (str->size != 0 && str->data != buf) {
		/* Move the remains of old chunk to start of `buf'. */
		memmove(buf, str->data, str->size);
	}

	const size_t n = fread(buf + str->size, 1, bufsize - str->size, input);
	if (n == 0) {
		if (feof(input)) {
			str->data = NULL;
		} else if (ferror(input)) {
			strncpy((char *) buf, strerror(errno), bufsize);
			str->data = buf;
		} else {
			assert(0 == 1);
		}

		str->type = S_EOF;
		str->size = 0; /* Yes, even for error messages. */
	} else {
		str->type = S_CHUNK;
		str->data = buf;
		str->size += n;
	}

	return str->size;
}

/* ---------------------------------------------------------------------
 * Iteratees
 */

enum { IE_CONT, IE_DONE };

static int
_count_A(unsigned int *z, struct Stream *str)
{
	switch (str->type) {
	case S_CHUNK:
		for (; str->size > 0; ++str->data, --str->size) {
			if (*str->data == 'A')
				++(*z);
		}
		return IE_CONT;

	case S_EOF:
		return IE_DONE;

	default:
		assert(0 == 1);
	}
}

#if 0 /*XXX*/
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

	/* Tag class:  Universal | Application | Context-specific | Private */
	putchar("uacp"[(c & 0xc0) >> 6]);

	if ((c & 0x1f) == 0x1f)
		UNDEFINED; /* tag number > 30 */
	else
		printf("%d ", c & 0x1f);

	puts(c & 0x20 ? "<XXX_constructed>)" : "<XXX_primitive>)");

	return 0;
}
#endif /*XXX*/

/* ---------------------------------------------------------------------
 * Enumerator
 */

static int
traverse(const char *inpath, struct MemBuf *buf)
{
	FILE *f = NULL;

	if (streq(inpath, "-")) {
		f = stdin;
	} else if ((f = fopen(inpath, "rb")) == NULL) {
		error(0, errno, inpath);
		return -1;
	}

	if (adjust_buffer(f, buf) < 0) {
		error(0, errno, inpath);
		return -1;
	}
	size_t filepos = 0;

	struct Stream str = { S_CHUNK, buf->data, 0 };

	unsigned int z = 0; /* XXX sum of 'A' characters */
	for (;;) {
		const size_t orig_size = \
			retrieve(&str, f, buf->data, buf->size);

		const int indic = _count_A(&z, &str);
		assert(indic == IE_DONE || indic == IE_CONT);

		debug_print("%s:%ld: IE_%s (z = %d)", inpath,
			    (unsigned long) filepos,
			    indic == IE_DONE ? "DONE" : "CONT", z);

		if (str.size == orig_size && indic == IE_CONT)
			die("Iteratee has consumed nothing, but asks for"
			    " more.\nAborting to prevent an endless loop.");

		filepos += orig_size - str.size;

		if (indic == IE_DONE)
			break;
	}
	debug_print("%s contains %d 'A' characters", inpath, z);

	return streq(inpath, "-") ? 0 : fclose(f);
}

int
main(int argc, char **argv)
{
	struct MemBuf buf = { NULL, 0 };

	int rv = 0;
	if (argc == 1) {
		rv = traverse("-", &buf);
	} else {
		int i;
		for (i = 1; i < argc; i++)
			rv |= traverse(argv[i], &buf);
	}

	free(buf.data);
	return -rv;
}
