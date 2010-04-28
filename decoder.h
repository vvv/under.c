#ifndef _DECODER_H
#define _DECODER_H

#include "list.h"
#include "iteratee.h"

/* Decoding state. */
struct DecSt {
	unsigned int depth; /* Current depth within tag hierarchy. */

	/*
	 * List of remaining capacities.
	 *
	 * Capacity here is the number of bytes left in given
	 * container.
	 */
	struct list_head caps;
};
#define DECST_INIT(name) { 0, LIST_HEAD_INIT(name.caps) }

/*
 * Decode DER data.  [enumeratee]
 *
 * @z: seed
 * @master: master stream
 */
IterV decode(struct DecSt *z, struct Stream *master);


#endif /* _DECODER_H */
