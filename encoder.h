#ifndef _ENCODER_H
#define _ENCODER_H

#include "util.h"
#include "list.h"
#include "iteratee.h"

/* State of encoder */
struct EncSt {
	struct Pstring acc; /* Encoded bytes' accumulator */

	/*
	 * Backtrace -- a summary of how encoder got where it is.
	 *
	 * It starts with the current node, followed by its parent,
	 * and on up the stack (to the root).
	 */
	struct list_head bt;
};

/* XXX */
void init_EncSt(struct EncSt *z);

/* XXX */
void free_EncSt(struct EncSt *z);

/* XXX */
IterV encode(struct EncSt *z, struct Stream *str);

#endif /* _ENCODER_H */
