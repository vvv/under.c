#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include "util.h"
#include "stack.h"

#ifdef DEBUG
#  define UNDEFINED error_at_line(1, 0, __FILE__, __LINE__, \
				  "XXX not implemented")
#  define debug_print(format, ...) \
	fprintf(stderr, "(DEBUG) " format "\n", ##__VA_ARGS__)
#else
#  define debug_print(...)
#endif

/* Memory buffer. */
struct MemBuf {
	char *data; /* Pointer to malloc(3)-ated memory. */
	size_t size; /* Size of buffer. */
};

static struct MemBuf errbuf = { NULL, 0 };

/*
 * Adjust buffer to file's blocksize.
 *
 * Do nothing if the size of buffer is equal to blocksize of file.
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
 * A stream is a (continuing) sequence of bytes bundled in Chunks.
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
	 * Zero length signifies a stream with no currently available
	 * data but which is still continuing. A stream processor
	 * should, informally speaking, ``suspend itself'' and wait
	 * for more data to arrive.
	 *
	 * Meaningless for S_EOF.
	 */
	size_t size;

	/*
	 * Error message (S_EOF only).
	 *
	 * NULL if EOF was reached without error; otherwise this is a
	 * pointer to an error message (or a control message in
	 * general).
	 *
	 * Meaningless for S_CHUNK.
	 */
	const char *errmsg;
};
#define STREAM_INIT { S_EOF, NULL, 0, NULL }

static void
set_error(const char **errmsg, const char *format, ...)
{
	if (*errmsg != NULL)
		return; /* keep the original error message */

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

	*errmsg = errbuf.data;
}

/*
 * Read another block of data from an input file.
 *
 * @dest: memory buffer to read into
 * @src: file to read from
 * @stream: a stream to update */
static size_t
read_block(struct MemBuf *dest, FILE *src, struct Stream *stream)
{
	const size_t n = fread(dest->data, 1, dest->size, src);
	if (n == 0) {
		stream->type = S_EOF;

		if (feof(src)) {
			debug_print("read_block: EOF");
			stream->errmsg = NULL;
		} else if (ferror(src)) {
			debug_print("read_block: IO error");
			set_error(&stream->errmsg, strerror(errno));
		} else {
			assert(0 == 1);
		}

		return 0;
	}

	debug_print("read_block: %lu bytes read", (unsigned long) n);
	stream->type = S_CHUNK;
	stream->data = (unsigned char *) dest->data;
	stream->size = n;

	return n;
}

/* ---------------------------------------------------------------------
 * Iteratees
 */

typedef enum { IE_CONT, IE_DONE } IterV;

/*
 * Attempt to read the next byte of the stream and store it in `*c'.
 * Set an error if the stream is terminated.
 */
static IterV
head(unsigned char *c, struct Stream *str)
{
	if (str->type == S_EOF) {
		set_error(&str->errmsg, "head: EOF");
		return IE_CONT;
	}

	if (str->size == 0)
		return IE_CONT;

	*c = *str->data;
	++str->data;
	--str->size;

	return IE_DONE;
}

/*
 * Attributes of tag contents.
 *
 * Values of this type are filled by `header' iteratee.
 *
 * XXX Replace with `Header' type?
 */
struct Cont_Attr {
	bool cons_p; /* Is encoding constructed? */
	size_t len; /* Length of contents. */
};

/*
 * Parse tag header: print its representation and get some info about
 * tag contents.
 *
 * XXX TODO  Separate parsing of header from ``side effects'' (printing).
 */
static IterV
header(struct Cont_Attr *dest, struct Stream *str)
{
	static int cont = 0; /* Point to continue execution from */
	static unsigned int tagnum; /* Tag number */
	static size_t len_sz; /* Number of length octets, excluding initial */

	unsigned char c;

	switch (cont) {
	case 0: /* Identifier octet(s) -- cases 0..2 */
		if (str->type == S_EOF)
			return IE_DONE;

		if (head(&c, str) == IE_CONT)
			return IE_CONT;
		putchar('(');

		/*
		 * Tag class:
		 * Universal | Application | Context-specific | Private
		 */
		putchar("uacp"[(c & 0xc0) >> 6]);

		dest->cons_p = (c & 0x20) != 0;

		if ((c & 0x1f) == 0x1f) {
			tagnum = 0; /* tag number > 30 */
		} else {
			tagnum = c & 0x1f;
			goto tagnum_done;
		}

	case 1: /* Tag number > 30 (``high'' tag number) */
		cont = 1;
		for (; str->size > 0 && *str->data & 0x80;
		     ++str->data, --str->size)
			tagnum = (tagnum << 7) | (*str->data & 0x7f);

		if (head(&c, str) == IE_CONT)
			return IE_CONT;
		tagnum = (tagnum << 7) | (c & 0x7f);

tagnum_done:
		if (tagnum > 1000) {
			set_error(&str->errmsg,
				  "XXX Tag number is too big: %u", tagnum);
			return IE_CONT;
		}
		printf("%u", tagnum);

	case 2: /* Initial length octet */
		cont = 2;
		if (head(&c, str) == IE_CONT)
			return IE_CONT;

		if (c == 0xff) {
			set_error(&str->errmsg, "Length encoding is invalid\n"
				  "  [ITU-T X.690, 8.1.3.5-c]");
			return IE_CONT;
		}

		if (c & 0x80) {
			len_sz = c & 0x7f; /* long form */
			dest->len = 0;
		} else {
			dest->len = c; /* short form*/
			break;
		}

	case 3: /* Subsequent length octet(s) */
		cont = 3;
		for (; str->size > 0 && len_sz > 0;
		     --len_sz, ++str->data, --str->size)
			dest->len = (dest->len << 8) | *str->data;

		if (len_sz > 0)
			return IE_CONT;
		break;

	default:
		assert(0 == 1);
	}

	cont = 0;
	return IE_DONE;
}

/*
 * Print hex dump of primitive encoding.
 *
 * @enough: Are there enough bytes in `str' to reach the end of tag?
 */
static IterV
prim(bool enough, struct Stream *str)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		putchar('"');

	case 1:
		cont = 1;
		{
			unsigned char c;
			if (head(&c, str) == IE_CONT) {
				if (!enough)
					return IE_CONT;
				break; /* ``empty'' tag  (clen == 0) */
			}
			printf("%02x", c);
		}

	case 2:
		cont = 2;
		for (; str->size > 0; ++str->data, --str->size)
			printf(" %02x", *str->data);

		if (!enough)
			return IE_CONT;
		break;

	default:
		assert(0 == 1);
	}

	putchar('"');

	cont = 0;
	return IE_DONE;
}

static void
advance(size_t n, const struct Stream *src, struct Stream *master,
	struct Stack *caps)
{
	for (; caps != NULL; caps = caps->next)
		caps->value -= n; /* decrease capacities */

	master->data = src->data;
	master->size -= n;
	master->errmsg = src->errmsg;
}

/*
 * Is there enough capacity for that many bytes?
 *
 * @n: number of bytes
 */
static inline bool
contained_p(size_t n, const struct Stack *caps)
{
	/* NULL signifies infinite capacity */
	return caps == NULL ? true : caps->value >= n;
}

/* ---------------------------------------------------------------------
 * Enumeratee
 */

struct Caps {
	unsigned int depth; /* Current depth within tag hierarchy. */

	/*
	 * A list of remaining capacities.
	 *
	 * A capacity here is the number of bytes that should be
	 * consumed by a parser that operates at given level of tag
	 * hierarchy.
	 */
	struct Stack *caps;
};
#define CAPS_INIT { 0, NULL }

#ifdef DEBUG
static void
check_caps_invariant(const struct Caps *x)
{
	unsigned int d;
	const struct Stack *p;

	for (d = x->depth, p = x->caps; d > 0 && p != NULL;
	     --d, p = p->next)
		assert(p->next == NULL || p->value <= p->next->value);

	assert(d == 0 && p == NULL);
}
#else
#  define check_caps_invariant(...)
#endif

#ifdef DEBUG
static inline void
debug_traversal_position(const struct Caps *z, const struct Stream *str,
			 const struct Stream *master)
{
	fprintf(stderr, "(DEBUG) traverse: %lu/%lu %u [",
		(unsigned long) str->size, (unsigned long) master->size,
		z->depth);

	if (z->caps != NULL) {
		fprintf(stderr, "%u", z->caps->value);

		const struct Stack *p = z->caps->next;
		for (; p != NULL; p = p->next)
			fprintf(stderr, ",%u", p->value);
	}

	fputs("]\n", stderr);
}
#else
#  define debug_traversal_position(...)
#endif

/*
 * XXX
 *
 * @master: master stream
 */
static IterV
traverse(struct Caps *z, struct Stream *master)
{
	static bool header_p = true; /* Do we parse tag header at this step? */
	static struct Stream str; /* Substream, passed to an iteratee */

	if (master->type == S_EOF)
		return IE_DONE;

	str.type = master->type;
	str.data = master->data;
	str.errmsg = master->errmsg;

	for (;;) {
		str.size = z->caps == NULL ?
			master->size : MIN(z->caps->value, master->size);
		debug_traversal_position(z, &str, master); /* XXX */

		const size_t orig_size = str.size;
		struct Cont_Attr next_cont;

		const int indic = header_p ?
			header(&next_cont, &str) :
			prim(z->caps->value <= str.size, &str);
		assert(indic == IE_DONE || indic == IE_CONT);

		advance(orig_size - str.size, &str, master, z->caps);
		debug_traversal_position(z, &str, master); /* XXX */

		if (indic == IE_CONT)
			return IE_CONT;

		/* IE_DONE */
		while (z->caps != NULL && z->caps->value == 0) {
			stack_pop(&z->caps);
			--z->depth;
			putchar(')');
		}
		check_caps_invariant(z);

		if (header_p) {
			if (!contained_p(next_cont.len, z->caps)) {
				set_error(&master->errmsg,
					  "Tag is too big for its container");
				return IE_CONT;
			}
			stack_push(&z->caps, next_cont.len);
			++z->depth;

			if (!next_cont.cons_p) {
				header_p = false;
				putchar(' ');
				continue;
			}
		} else {
			header_p = true;
		}

		putchar('\n');
		unsigned int i;
		for (i = 0; i < z->depth; ++i)
			fputs("    ", stdout);
	}
}


/* ---------------------------------------------------------------------
 * Enumerator
 */

/*
 * XXX
 *
 * Return value: 0 -- successful completion, -1 -- error.
 */
static int
process(const char *inpath, struct MemBuf *buf)
{
	FILE *f = NULL;

	if (streq(inpath, "-")) {
		f = stdin;
	} else if ((f = fopen(inpath, "rb")) == NULL) {
		error(0, errno, "%s", inpath);
		return -1;
	}

	if (adjust_buffer(f, buf) < 0) {
		error(0, errno, "%s", inpath);
		return -1;
	}

	int retval = 0;
	size_t filepos = 0;
	struct Stream str = STREAM_INIT;
	struct Caps z = CAPS_INIT;

	for (;;) {
		const size_t orig_size = read_block(buf, f, &str);
		assert(str.type == S_CHUNK || (str.type == S_EOF &&
					       orig_size == 0));

		if (str.type == S_EOF) {
			if (str.errmsg != NULL) {
				error_at_line(0, 0, inpath, filepos,
					      "%s", str.errmsg);
				retval = -1;
			} else if (z.depth > 0) {
				error_at_line(0, 0, inpath, filepos,
					      "Unexpected EOF");
				retval = -1;
			}
			break;
		}

		const int indic = traverse(&z, &str);
		assert(indic == IE_DONE || indic == IE_CONT);

		filepos += orig_size - str.size;

		if (indic == IE_DONE)
			break;

		/* IE_CONT */
		if (str.errmsg != NULL) {
			error_at_line(0, 0, inpath, filepos, "%s", str.errmsg);
			retval = -1;
			break;
		} else {
			assert(str.size == 0);
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
		rv = process("-", &buf);
	} else {
		int i;
		for (i = 1; i < argc; i++)
			rv |= process(argv[i], &buf);
	}

	free(buf.data);
	free(errbuf.data);
	return -rv;
}
