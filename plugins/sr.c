/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "ctt.h"
#include "../buffer.h"
#include "../util.h"

int
decode_CallTransactionType(struct Buffer *dest, const uint8_t *src, size_t n)
{
	if (n != 1) {
		buffer_xprintf(dest, "decode_CallTransactionType: 1 byte"
			       " expected, %lu received", (unsigned long) n);
		return -1;
	}

	const uint8_t x = *src;

	if (x >= ARRAY_SIZE(ctt_idx) || ctt_idx[x] == 0) {
		buffer_xprintf(dest, "decode_CallTransactionType: unsupported"
			       " value: %u", x);
		return -1;
	}

	return buffer_printf(dest, "%s (%u)", ctt_dict[ctt_idx[x]].symbol, x);
}

#if 0 /*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
static inline int
_ctt_cmp(const void *k, const void *m)
{
	return strcmp(k, ((const struct CTT_Pair *) m)->symbol);
}

int
_ctt_tonum(const char *src, uint8_t *dest)
{
	const struct CTT_Pair *p =
		bsearch(src, ctt_dict + 1, ARRAY_SIZE(ctt_dict) - 1,
			sizeof(*ctt_dict), _ctt_cmp);
	if (p == NULL)
		return -1;

	*dest = p->number;
	return 0;
}
#endif /*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX*/
