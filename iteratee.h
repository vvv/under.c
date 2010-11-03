/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ITERATEE_H
#define _ITERATEE_H

/*
 * Iteratee-based I/O
 *
 * In essence, an iteratee takes a chunk of the input stream and
 * returns one of:
 * - the computed result and the remaining part of the stream;
 * - the indication that iteratee needs more data, along with the
 *   continuation to process these data;
 * - a message to the stream producer (e.g., to rewind the stream) or
 *   an error indicator.
 *
 * See also:
 *     http://okmij.org/ftp/papers/LL3-collections-enumerators.txt
 *     http://okmij.org/ftp/Streams.html
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum { IE_CONT, IE_DONE } IterV;

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
	const uint8_t *data;

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
	 * Error message.
	 *
	 * NULL if EOF was reached without error; otherwise this is a
	 * pointer to an error message (or a control message in general).
	 */
	char *errmsg;
};
#define STREAM_INIT { S_EOF, NULL, 0, NULL }

/* -- Some primitive iteratees -------------------------------------- */

/*
 * Attempt to read the next byte of the stream and store it in `*c'.
 * Set an error if the stream is terminated.
 */
IterV head(uint8_t *c, struct Stream *str);

/* Skip prefix bytes that satisfy the predicate. */
IterV drop_while(bool (*p)(uint8_t c), struct Stream *str);

#endif /* _ITERATEE_H */
