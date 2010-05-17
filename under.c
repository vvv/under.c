#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include "util.h"
#include "decoder.h"
#include "encoder.h"

/*
 * Adjust buffer to file's blocksize.
 *
 * Do nothing if the size of buffer is equal to blocksize of file.
 * Otherwise (re)allocate memory.
 */
static int
adjust_buffer(FILE *f, struct Pstring *buf)
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

/*
 * Read another block of data from an input file.
 *
 * @dest: memory buffer to read into
 * @src: file to read from
 * @stream: a stream to update */
static size_t
read_block(struct Pstring *dest, FILE *src, struct Stream *stream)
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

/*
 * Enumerator, XXX.
 *
 * Return value: 0 -- successful completion, -1 -- error.
 */
static int
process(const char *inpath, struct Pstring *inbuf, unsigned char *encbuf,
	size_t encbuf_size)
{
	FILE *f = NULL;

	if (streq(inpath, "-")) {
		f = stdin;
	} else if ((f = fopen(inpath, "rb")) == NULL) {
		error(0, errno, "%s", inpath);
		return -1;
	}

	if (adjust_buffer(f, inbuf) < 0) {
		error(0, errno, "%s", inpath);
		return -1;
	}

	int retval = 0;
	size_t filepos = 0;
	struct Stream str = STREAM_INIT;
#if 0 /*XXX*/
	struct DecSt z = DECST_INIT(z);
#else /*XXX*/
	struct EncSt z = ENCST_INIT(encbuf_size, encbuf, z);
#endif /*XXX*/

	for (;;) {
		const size_t orig_size = read_block(inbuf, f, &str);
		assert(str.type == S_CHUNK || (str.type == S_EOF &&
					       orig_size == 0));

		if (str.type == S_EOF && str.errmsg != NULL) {
			error_at_line(0, 0, inpath, filepos, "%s", str.errmsg);
			retval = -1;
			break;
		}

#if 0 /*XXX*/
		const IterV indic = decode(&z, &str);
#else /*XXX*/
		const IterV indic = encode(&z, &str);
#endif /*XXX*/
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

	free(str.errmsg);
	return retval;
}

int
main(int argc, char **argv)
{
	struct Pstring inbuf = PSTRING_INIT;
	unsigned char encbuf[512] = {0}; /* XXX not needed for decoding */

	int rv = 0;
	if (argc == 1) {
		rv = process("-", &inbuf, encbuf, sizeof(encbuf));
	} else {
		int i;
		for (i = 1; i < argc; i++)
			rv |= process(argv[i], &inbuf, encbuf, sizeof(encbuf));
	}

	free(inbuf.data);
	return -rv;
}
