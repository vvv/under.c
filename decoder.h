/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
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

	/*
	 * Pointer to a structure that specifies how to convert tags
	 * to their human-readable representations (and vice versa).
	 * The structure is opaque for `decoder.c'.
	 */
	const struct Format_Repr *repr;
};

static inline void
init_DecSt(struct DecSt *z, const struct Format_Repr *repr)
{
	z->depth = 0;
	INIT_LIST_HEAD(&z->caps);
	z->repr = repr;
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
