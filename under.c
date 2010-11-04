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
#include "buffer.h"
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
adjust_buffer(struct Buffer *buf, FILE *f)
{
#ifdef DEBUG
	(void) f;
#  warning "Using buffer of size 5 for tests"
	return buffer_resize(buf, 5);
#else
	struct stat st;
	size_t insize;

	if (fstat(fileno(f), &st) != 0) {
		error(0, errno, "fstat failed");
		return -1;
	} else if ((insize = st.st_blksize) == buf->_max_size) {
		return 0;
	} else if (fstat(fileno(stdout), &st) != 0) {
		error(0, errno, "fstat(stdout) failed");
		return -1;
	} else {
		return buffer_resize(buf, MAX(insize, (size_t) st.st_blksize));
	}
#endif
}

/*
 * Read another block of data from an input file.
 *
 * @dest: memory location to store data at
 * @size: maximum number of bytes to read
 * @src: stream to read data from
 * @str: where to put error message
 *
 * Return the number of bytes read. Zero is returned if the
 * end-of-file is reached or an error occurs.
 */
static size_t
read_block(void *dest, size_t size, FILE *src, struct Stream *str)
{
	const size_t n = fread(dest, 1, size, src);

	if (n == 0) {
		if (feof(src)) {
			debug_print("read_block: EOF");
			assert(str->errmsg == NULL);
			str->errmsg = NULL;
		} else if (ferror(src)) {
			debug_print("read_block: IO error");
			set_error(str, strerror(errno));
		} else {
			assert(0 == 1);
		}
	}

	debug_print("read_block: %lu bytes read", (unsigned long) n);
	return n;
}

/*
 * This function is an /enumerator/ in the terminology of iteratees
 * [http://okmij.org/ftp/Streams.html].
 *
 * Return value: 0 - success, -1 - error.
 */
static int
process_file(enum Codec_T ct, const char *inpath, struct Buffer *inbuf,
	     const struct Repr_Format *repr)
{
	debug_print("process_file: \"%s\"", inpath);
	FILE *f = NULL;

	if (streq(inpath, "-")) {
		f = stdin;
	} else if ((f = fopen(inpath, "rb")) == NULL) {
		error(0, errno, "%s", inpath);
		return -1;
	}

	if (adjust_buffer(inbuf, f) < 0) {
		error(0, errno, "%s", inpath);
		return -1;
	}

	int retval = -1;
	size_t filepos = 0;
	struct Stream str = STREAM_INIT;
	void *z = NULL;

	for (;;) {
		const size_t orig_size = read_block(inbuf->wptr, inbuf->size,
						    f, &str);
		str.type = ((str.size = orig_size) == 0) ? S_EOF : S_CHUNK;
		str.data = inbuf->wptr;

		if (str.type == S_EOF && str.errmsg != NULL) {
			error_at_line(0, 0, inpath, filepos, "%s", str.errmsg);
			break;
		}

		const IterV indic = run_codec(ct, &z, &str, repr);
		assert(indic == IE_DONE || indic == IE_CONT);
		filepos += orig_size - str.size;

		if (indic == IE_CONT && str.errmsg != NULL) {
			error_at_line(0, 0, inpath, filepos, "%s", str.errmsg);
			break;
		}

		assert(str.size == 0);

		if (indic == IE_DONE) {
			retval = 0;
			break;
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
	enum Codec_T ct = DECODER;
	REPR_FORMAT(repr);
	BUFFER(inbuf);

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
			if (repr.dict != NULL) {
				repr_destroy(&repr);
				die("Multiple -f/--format options are not"
				    " allowed");
			}

			if (repr_create(&repr, optarg) != 0) {
				repr_destroy(&repr);
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
		rv = process_file(ct, "-", &inbuf, &repr);
	} else {
		int i;
		for (i = optind; i < argc; ++i)
			rv |= process_file(ct, argv[i], &inbuf, &repr);
	}

	repr_destroy(&repr);
	free(buffer_data(&inbuf));
	return -rv;
}
