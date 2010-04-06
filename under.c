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

#ifdef DEBUG
#  warning "Using buffer of size 5 for tests"
		buf->size = 5;
#else
		buf->size = MAX(insize, outsize);
#endif
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
		debug_print("retrieve: moving %d remaining bytes to"
			    " the start of buffer", str->size);
		/* Move the remains of old chunk to the start of `buf'. */
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
		str->size = 0; /* `size' is 0 for error messages (by design) */
	} else {
		str->type = S_CHUNK;
		str->data = buf;
		str->size += n;
	}

	debug_print("retrieve: %d bytes read", str->size);
	return str->size;
}

/* ---------------------------------------------------------------------
 * Iteratees
 */

enum { IE_CONT, IE_DONE };

struct TagParsingState {
	/* ``Continuation'': the point in `_tag' to continue execution from. */
	int cont;

	/* Whether encoding is constructed. */
	unsigned char cons_p;

	unsigned int tagnum; /* Tag number. */
	size_t nbytes_len; /* Number of length octets (excluding initial). */
	size_t len; /* Tag length. */
};

static int
_tag(struct TagParsingState *z, struct Stream *str)
{
	assert(str->type == S_CHUNK || str->type == S_EOF);

	switch (z->cont) {
	case 0: /* Identifier octet(s) -- cases 0..2 */
		if (str->type == S_EOF)
			return IE_DONE;

		if (str->size == 0)
			return IE_CONT;
		{
			const unsigned char c = *str->data;
			++str->data;
			--str->size;

			putchar('(');

			/*
			 * Tag class:
			 * Universal | Application | Context-specific | Private
			 */
			putchar("uacp"[(c & 0xc0) >> 6]);

			z->cons_p = (c & 0x20) != 0;

			if ((c & 0x1f) == 0x1f) {
				z->tagnum = 0; /* tag number > 30 */
			} else {
				z->tagnum = c & 0x1f;
				goto tagnum_done;
			}
		}

	case 1: /* Tag number > 30 (a.k.a. high tag number) */
		for (; str->size > 0 && *str->data & 0x80;
		     ++str->data, --str->size)
			z->tagnum = (z->tagnum << 7) | (*str->data & 0x7f);

		if (str->size == 0) {
			z->cont = *str->data & 0x80 ? 1 : 2;
			return IE_CONT;
		}

	case 2: /* Tag number > 30, continued */
		z->tagnum = (z->tagnum << 7) | (*str->data & 0x7f);
		++str->data;
		--str->size;

tagnum_done:
		printf("%d ", z->tagnum);

	case 3: /* Length octet(s) -- cases 3..4 */
		if (str->size == 0) {
			z->cont = 3;
			return IE_CONT;
		}

		{
			/* XXX `head' iteratee is needed */
			const unsigned char c = *str->data;
			++str->data;
			--str->size;

			if (c == 0xff) {
				UNDEFINED;
				/* set_error_XXX("length encoding is invalid\n" */
				/* 	      "\t[ITU-T X.690, 8.1.3.5-c]"); */
				/* return IE_CONT; */
			}

			if (c & 0x80) {
				z->nbytes_len = c & 0x7f;
				z->len = 0;
			} else {
				z->len = c;
				goto len_done;
			}
		}

	case 4:
		for (; str->size > 0 && z->nbytes_len > 0;
		     --z->nbytes_len, ++str->data, --str->size)
			z->len = (z->len << 8) | *str->data;

		if (z->nbytes_len > 0) {
			z->cont = 4;
			return IE_CONT;
		}

len_done:
		printf("<l=%d>", z->len);

		if (z->cons_p) {
			/* Constructed encoding */
			fputs("<XXX constructed> \"", stdout);
		} else {
			/* Primitive encoding */
			fputs("<XXX primitive> \"", stdout);
		}

	case 5: /* Contents octet(s) */
		for (; str->size > 0 && z->len > 0;
		     --z->len, ++str->data, --str->size)
			printf(" %02x", *str->data);

		if (z->len > 0) {
			z->cont = 5;
			return IE_CONT;
		}

		puts("\")");

		break; /* done with tag */

	default:
		debug_print("**ERROR** _tag: unexpected continuation: %d",
			    z->cont);
		assert(0 == 1);
		return -1;
	}

	z->cont = 0;
	return IE_DONE;
}

static int
_tags(struct TagParsingState *z, struct Stream *str)
{
	assert(str->type == S_CHUNK || str->type == S_EOF);

	int indic;
	do {
		indic = _tag(z, str);
		assert(indic == IE_DONE || indic == IE_CONT);

		debug_print("IE_%s {cont=%d, cons_p=%d, tagnum=%d,"
			    " nbytes_len=%d, len=%d}",
			    indic == IE_DONE ? "DONE" : "CONT",
			    z->cont, z->cons_p, z->tagnum, z->nbytes_len,
			    z->len);
	} while (indic == IE_DONE && str->type == S_CHUNK);

	return indic;
}

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

	struct TagParsingState z = {0,0,0,0,0};
	for (;;) {
		const size_t orig_size = \
			retrieve(&str, f, buf->data, buf->size);

		const int indic = _tags(&z, &str);
		assert(indic == IE_DONE || indic == IE_CONT);

		if (str.size == orig_size && indic == IE_CONT)
			die("Iteratee has consumed nothing, but asks for"
			    " more.\nAborting to prevent an endless loop.");

		filepos += orig_size - str.size;

		if (indic == IE_DONE)
			break;
	}

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
