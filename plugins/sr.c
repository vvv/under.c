/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include "../util.h"
#include "ctt.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * Decode binary DER data to human-readable representation.
 *
 * @src: data to decode
 * @dest: resulting string or error message
 * @n: capacity of `dest'
 *
 * Function writes at most `n' bytes, including terminating '\0', to
 * `dest'.
 *
 * Return value:
 *    0 - success;
 *   -1 - decoding has failed or resulting string exceeds `n' bytes;
 *        `dest' will contain error message in this case.
 */
int
decode_callTransactionType(const struct Pstring *src, char *dest, size_t n)
{
	if (src->size != 1) {
		snprintf(dest, n, "decode_callTransactionType: 1 byte expected"
			 ", %lu received", (unsigned long) src->size);
		return -1;
	}

	const uint8_t x = *src->data;

	if (x >= ARRAY_SIZE(ctt_idx) || ctt_idx[x] == 0) {
		snprintf(dest, n, "decode_callTransactionType:"
			 " unsupported value (%u)", x);
		return -1;
	}

	return ((size_t) snprintf(dest, n, "%s (%u)",
				  ctt_dict[ctt_idx[x]].symbol, x) < n) ?
		0 : -1;
}

/* int */
/* encode_callTransactionType(const struct Pstring *src, struct Pstring *dest) */
/* { */
/* } */

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
