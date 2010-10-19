/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _CODEC_H
#define _CODEC_H

#include "iteratee.h"

struct hlist_head;

/* Type of codec */
enum Codec_T { DECODER, ENCODER };

/* XXX */
IterV run_codec(enum Codec_T type, void **z, struct Stream *str,
		const struct hlist_head *repr);

/* Release resources allocated for codec's state (z) */
void free_codec(enum Codec_T type, void *z);

#endif /* _CODEC_H */
