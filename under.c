#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdarg.h>

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
	char *data; /* pointer to malloc(3)-ated memory */
	size_t size; /* size of buffer */
};

static struct MemBuf errbuf = { NULL, 0 };

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
#ifdef DEBUG
#  warning "Using buffer of size 5 for tests"
		buf->size = 5;
#endif
		debug_print("adjust_buffer: realloc(%p, %lu)", buf->data,
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
	 * Pointer to chunk data (S_CHUNK only).
	 *
	 * Meaningless for S_EOF.
	 */
	const unsigned char *data;

	/*
	 * Size of chunk (S_CHUNK only).
	 *
	 * S_CHUNK: Zero length signifies a stream with no currently
	 * available data but which is still continuing. A stream
	 * processor should, informally speaking, ``suspend itself''
	 * and wait for more data to arrive.
	 *
	 * Meaningless for S_EOF.
	 */
	size_t size;

	/*
	 * Error message (S_EOF only).
	 *
	 * S_EOF: NULL if EOF was reached without error; otherwise
	 * `errmsg' is a pointer to an error message (or a control
	 * message in general).
	 *
	 * Meaningless for S_CHUNK.
	 */
	const char *errmsg;
};

static void
set_error(struct Stream *stream, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	const size_t nchars = vsnprintf(errbuf.data, errbuf.size, format, ap);
	if (nchars >= errbuf.size) {
		/* Not enough space.  Reallocate buffer .. */
		errbuf.size = nchars + 1;
		errbuf.data = xrealloc(errbuf.data, errbuf.size);

		 /* .. and try again. */
		vsnprintf(errbuf.data, errbuf.size, format, ap);
	}

	va_end(ap);

	stream->type = S_EOF;
	stream->errmsg = errbuf.data;
}

static size_t
retrieve(struct Stream *str, FILE *input, struct MemBuf *buf)
{
	assert(str->type == S_CHUNK); /* XXX And what about S_EOF? */

	/* A chunk must be contained in `buf'. */
	assert(buf->data <= (char *) str->data);
	assert((char *) str->data + str->size <= buf->data + buf->size);

	if (str->size != 0 && (char *) str->data != buf->data) {
		debug_print("retrieve: %lu remaining bytes moved to the start"
			    " of buffer", (unsigned long) str->size);
		/* Move the remains of old chunk to the start of `buf'. */
		memmove(buf->data, str->data, str->size);
	}

	const size_t n = fread(buf->data + str->size, 1,
			       buf->size - str->size, input);
	if (n == 0) {
		if (feof(input)) {
			debug_print("retrieve: EOF");
			str->type = S_EOF;
			str->errmsg = NULL;
		} else if (ferror(input)) {
			debug_print("retrieve: IO error");
			set_error(str, strerror(errno));
		} else {
			assert(0 == 1);
		}

		return 0;
	}

	debug_print("retrieve: %lu bytes read", (unsigned long) n);
	str->type = S_CHUNK;
	str->data = (unsigned char *) buf->data;
	str->size += n;

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

static inline int
_tag__cont(int mark, struct TagParsingState *z)
{
	z->cont = mark;
	return IE_CONT;
}

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

	case 1: /* Tag number > 30 (a.k.a. ``high'' tag number) */
		for (; str->size > 0 && *str->data & 0x80;
		     ++str->data, --str->size)
			z->tagnum = (z->tagnum << 7) | (*str->data & 0x7f);

		if (str->size == 0)
			return _tag__cont(1, z);

		z->tagnum = (z->tagnum << 7) | (*str->data & 0x7f);
		++str->data;
		--str->size;

tagnum_done:
		if (z->tagnum > 1000) {
			set_error(str, "XXX Tag number is too big: %u",
				  z->tagnum);
			return IE_CONT;
		}
		printf("%u ", z->tagnum);

	case 2: /* Initial length octet */
		if (str->size == 0)
			return _tag__cont(2, z);

		{
			/* XXX `head' iteratee is needed */
			const unsigned char c = *str->data;
			++str->data;
			--str->size;

			if (c == 0xff) {
				set_error(str, "Length encoding is invalid\n"
					  "\t[ITU-T X.690, 8.1.3.5-c]");
				return IE_CONT;
			}

			if (c & 0x80) {
				z->nbytes_len = c & 0x7f;
				z->len = 0;
			} else {
				z->len = c;
				goto len_done;
			}
		}

	case 3: /* Subsequent length octet(s) */
		for (; str->size > 0 && z->nbytes_len > 0;
		     --z->nbytes_len, ++str->data, --str->size)
			z->len = (z->len << 8) | *str->data;

		if (z->nbytes_len > 0)
			return _tag__cont(3, z);

len_done:
		printf("<l=%lu>", (unsigned long) z->len);

		if (z->cons_p) {
			/* Constructed encoding */
			fputs("<XXX constructed> \"", stdout);
		} else {
			/* Primitive encoding */
			fputs("<XXX primitive> \"", stdout);
		}

	case 4: /* Contents octet(s) */
		for (; str->size > 0 && z->len > 0;
		     --z->len, ++str->data, --str->size)
			printf(" %02x", *str->data);

		if (z->len > 0)
			return _tag__cont(4, z);

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

		debug_print("IE_%s {cont=%d, cons_p=%d, tagnum=%u,"
			    " nbytes_len=%lu, len=%lu}",
			    indic == IE_DONE ? "DONE" : "CONT",
			    z->cont, z->cons_p, z->tagnum,
			    (unsigned long) z->nbytes_len,
			    (unsigned long) z->len);
	} while (indic == IE_DONE && str->type == S_CHUNK);

	return indic;
}

/* ---------------------------------------------------------------------
 * Enumerator
 */

/*
 * Return value: 0 -- successful completion, -1 -- error.
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

	int retval = 0;
	struct Stream str = { S_CHUNK, (unsigned char *) buf->data, 0, NULL };

	struct TagParsingState z = {0,0,0,0,0};
	for (;;) {
		const size_t orig_size = retrieve(&str, f, buf);
		assert(str.type == S_CHUNK || (str.type == S_EOF &&
					       orig_size == 0));

		if (str.type == S_EOF) {
			if (str.errmsg != NULL) {
				error_at_line(0, 0, inpath, filepos,
					      "%s", str.errmsg);
				retval = -1;
			} else if (z.cont != 0) {
				error_at_line(0, 0, inpath, filepos,
					      "Unexpected EOF");
				retval = -1;
			}
			break;
		}

		const int indic = _tags(&z, &str);
		assert(indic == IE_DONE || indic == IE_CONT);

		filepos += orig_size - str.size;

		if (indic == IE_DONE)
			break;

		/* IE_CONT */
		if (str.errmsg != NULL) {
			error_at_line(0, 0, inpath, filepos, "%s", str.errmsg);
			retval = -1;
			break;
		} else if (str.size == orig_size) {
			/*
			 * Iteratee is brain-damaged and should not be used,
			 * thus die.
			 */
			die("%s:%lu: Iteratee has consumed nothing, but wants"
			    " more.\n\tAborting to prevent an endless"
			    " loop.", inpath, (unsigned long) filepos);
		}
	}

	if (!streq(inpath, "-"))
		retval |= fclose(f);

	return retval;
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
	free(errbuf.data);
	return -rv;
}
