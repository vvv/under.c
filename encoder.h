#ifndef _ENCODER_H
#define _ENCODER_H

#include "list.h"
#include "iteratee.h"

/* State of encoder */
struct EncSt {
	/*
	 * Backtrace -- a summary of how encoder got where it is.
	 *
	 * It starts with the current node, followed by its parent,
	 * and on up the stack (to the root).
	 */
	struct list_head bt;
};
#define ENCST_INIT(name) { LIST_HEAD_INIT(name.bt) }

/* XXX */
IterV encode(struct EncSt *z, struct Stream *str);

#endif /* _ENCODER_H */
