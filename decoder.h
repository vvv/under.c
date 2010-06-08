#ifndef _DECODER_H
#define _DECODER_H

#include "list.h"
#include "iteratee.h"

/* Decoding state */
struct DecSt {
	uint32_t depth; /* Current depth within tag hierarchy */

	/*
	 * List of remaining capacities.
	 *
	 * Capacity here is the number of bytes left in given
	 * container (tag).
	 */
	struct list_head caps;
};

static inline void
init_DecSt(struct DecSt *z)
{
	z->depth = 0;
	INIT_LIST_HEAD(&z->caps);
}

/*
 * Decode DER data.
 *
 * @z: seed
 * @master: master stream
 *
 * This function is an enumeratee - iteratee and enumerator at the
 * same time - according to Oleg Kiselyov's terminology.
 * See <http://okmij.org/ftp/Haskell/Iteratee/IterateeM.hs>.
 */
IterV decode(struct DecSt *z, struct Stream *master);


#endif /* _DECODER_H */
