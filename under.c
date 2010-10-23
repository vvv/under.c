/*
 * under.c -- DER data decoder/encoder
 *
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <libgen.h>
#include <getopt.h>
#include "util.h"
#include "codec.h"
#include "repr.h"

#define VERSION "0.4.0"

/*
 * Adjust buffer to the blocksize of a file.
 *
 * Do nothing if size of buffer is equal to blocksize of file;
 * otherwise (re)allocate memory.
 */
static int
adjust_buffer(FILE *f, struct Pstring *buf)
{
#ifdef DEBUG
	do {} while (&f == NULL);
#  warning "Using buffer of size 5 for tests"
	buf->size = 5;

	debug_print("adjust_buffer: malloc(%lu)", (unsigned long) buf->size);
	buf->data = xmalloc(buf->size);
#else
	struct stat st;
	if (fstat(fileno(f), &st) < 0)
		return -1;
	const unsigned long insize = st.st_blksize;

	if (insize != buf->size) {
		if (fstat(fileno(stdout), &st) < 0) {
			error(0, errno, "fstat(STDOUT)");
			return -1;
		}

		buf->size = MAX(insize, (unsigned long) st.st_blksize);

		debug_print("adjust_buffer: realloc(%p, %lu)", buf->data,
			    (unsigned long) buf->size);
		buf->data = xrealloc(buf->data, buf->size);
	}
#endif
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
	stream->data = dest->data;
	stream->size = n;

	return n;
}

/*
 * This function is an /enumerator/ in the terminology of iteratees
 * [http://okmij.org/ftp/Streams.html].
 *
 * Return value: 0 - success, -1 - error.
 */
static int
process_file(enum Codec_T ct, const char *inpath, struct Pstring *inbuf,
	     const struct hlist_head *repr)
{
	debug_print("process_file: `%s'", inpath);
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
	void *z = NULL;

	for (;;) {
		const size_t orig_size = read_block(inbuf, f, &str);
		assert(str.type == S_CHUNK || (str.type == S_EOF &&
					       orig_size == 0));

		if (str.type == S_EOF && str.errmsg != NULL) {
			error_at_line(0, 0, inpath, filepos, "%s", str.errmsg);
			retval = -1;
			break;
		}

		const IterV indic = run_codec(ct, &z, &str, repr);
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
	free_codec(ct, z); /* XXX malloc/free for each input file is not good */
	return retval;
}

static void
usage(char *argv0)
{
	const char *s = basename(argv0);
	printf("Usage: %s [OPTION] [FILE]...\n"
	       "Decode DER from FILE(s), or standard input, to S-expressions.\n"
	       "\n"
	       "  -e, --encode   encode S-expressions to DER data\n"
	       "  -h, --help     display this help and exit\n"
	       "  -V, --version  output version information and exit\n"
	       "  -f, --format=NAME  interpret tags in accordance with"
	       " NAME.conf mapping\n"
	       "\n"
	       "With no FILE, or when FILE is -, read standard input.\n"
	       "\n"
	       "Examples:\n"
	       "  %s f - g  Decode f's contents, then standard input,"
	       " then g's contents.\n"
	       "  %s -e     Encode standard input to standard output.\n"
	       "\n"
	       "Report bugs to valery.vv@gmail.com\n"
	       "Home page: <http://github.com/vvv/under.c>\n", s, s, s);
}

int
main(int argc, char **argv)
{
	struct Pstring inbuf = { 0, NULL };
	enum Codec_T ct = DECODER;
	struct hlist_head *repr = NULL;

	const struct option longopts[] = {
		{ "encode", 0, NULL, 'e' },
		{ "format", 1, NULL, 'f' },
		{ "help", 0, NULL, 'h' },
		{ "version", 0, NULL, 'V' }
	};
	int c;
	while ((c = getopt_long(argc, argv, "ef:hV", longopts, NULL)) != -1) {
		switch (c) {
		case 'e':
			ct = ENCODER;
			break;

		case 'f':
			if (repr != NULL) {
				repr_destroy_htab(repr);
				error(1, 0, "Multiple -f/--format options are"
				      " not allowed");
			}

			repr = repr_create_htab();
			if (repr_read_conf(repr, optarg) != 0) {
				repr_destroy_htab(repr);
				return 1;
			}
			break;

		case 'h':
			usage(*argv);
			return 0;

		case 'V':
			printf("%s %s\n", basename(*argv), VERSION);
			return 0;

		default:
			return 1;
		}
	}

	int rv = 0;
	if (optind == argc) {
		rv = process_file(ct, "-", &inbuf, repr);
	} else {
		int i;
		for (i = optind; i < argc; ++i)
			rv |= process_file(ct, argv[i], &inbuf, repr);
	}

	repr_destroy_htab(repr);
	free(inbuf.data);
	return -rv;
}
