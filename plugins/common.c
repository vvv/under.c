/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdint.h>
#include <stdio.h>

#include "../buffer.h"

/* Telephony Binary Coded Decimal */
int
decode_TBCDstring(struct Buffer *dest, const uint8_t *src, size_t n)
{
	while (n != 0 && src[n - 1] == 0xff)
		--n; /* remove trailing fillers */

	for (; n != 0; --n, ++src) {
		const uint8_t msn = (*src & 0xf0) >> 4;
		const uint8_t lsn = *src &0xf;

		if (lsn >= 10 || (msn >= 10 && msn != 15)) {
			buffer_xprintf(dest, "Invalid TBCD byte: %02x", *src);
			return -1;
		}

		buffer_putc(dest, '0' + lsn);

		if (msn != 15) {
			buffer_putc(dest, '0' + msn);
			continue;
		}

		if (n > 1) {
			buffer_xprintf(dest, "Invalid sequence of TBCD bytes:"
				       " ..%02x %02x..", *src, src[1]);
			return -1;
		}
	}

	*dest->wptr = 0;
	return 0;
}
