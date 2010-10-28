/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>
#include "codec.h"
#include "decoder.h"
#include "encoder.h"

IterV
run_codec(enum Codec_T type, void **z, struct Stream *str,
	  const struct Format_Repr *repr)
{
	if (type == DECODER) {
		if (*z == NULL) {
			*z = xmalloc(sizeof(struct DecSt));
			init_DecSt(*z, repr);
		}
		return decode(*z, str);
	} else if (type == ENCODER) {
		if (*z == NULL) {
			*z = xmalloc(sizeof(struct EncSt));
			init_EncSt(*z);
		}
		return encode(*z, str);
	} else {
		assert(0 == 1);
	}
}

void
free_codec(enum Codec_T type, void *z)
{
	if (type == DECODER)
		free(z);
	else if (type == ENCODER)
		free_EncSt(z);
	else
		assert(0 == 1);
}
