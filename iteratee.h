#ifndef _ITERATEE_H
#define _ITERATEE_H

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
	 * Error message (S_EOF only).
	 *
	 * NULL if EOF was reached without error; otherwise this is a
	 * pointer to an error message (or a control message in
	 * general).
	 *
	 * Meaningless for S_CHUNK.
	 */
	char *errmsg;
};
#define STREAM_INIT { S_EOF, NULL, 0, NULL }

/*
 * Produce formatted error message.
 *
 * Do nothing if `*errmsg' is not NULL -- keep the original error
 * message.
 *
 * NOTE: This function calls malloc(3). Be sure to free(3) `*errmsg'
 * eventually.
 */
void set_error(char **errmsg, const char *format, ...);

/*
 * Attempt to read the next byte of the stream and store it in `*c'.
 * Set an error if the stream is terminated.
 */
IterV head(uint8_t *c, struct Stream *str);

/* Skip prefix bytes that satisfy the predicate. */
IterV drop_while(bool (*p)(uint8_t c), struct Stream *str);

#endif /* _ITERATEE_H */
