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

int decode_TBCDstring(struct Buffer *dest, const uint8_t *src, size_t n);

int
decode_BCDFlaggedString(struct Buffer *dest, const uint8_t *src, size_t n)
{
	if (decode_TBCDstring(dest, src + 1, n - 1) != 0)
		return -1;

	int rv = 0;
	uint8_t c = (*src >> 4) & 7;

	const char *type_of_number[] = {
		/* b000 */ "unknown",
		/* b001 */ "international",
		/* b010 */ "national",
		/* b011 */ "'network specific'",
		/* b100 */ "'dedicated access'"
	};

	if (c < ARRAY_SIZE(type_of_number))
		rv |= buffer_printf(dest, " type=%s", type_of_number[c]);
	else
		rv |= buffer_printf(dest, " type='reserved (0x%x)'", c);

	rv |= buffer_printf(dest, " plan=");

	switch (c = *src & 15) {
	case 0: return rv | buffer_printf(dest, "unknown");
	case 1: return rv | buffer_printf(dest, "ISDN/telephony");
	case 3: return rv | buffer_printf(dest, "data");
	case 4: return rv | buffer_printf(dest, "telex");
	case 8: return rv | buffer_printf(dest, "national");
	case 9: return rv | buffer_printf(dest, "private");
	default:;
	}

	return rv | buffer_printf(dest, "'reserved (0x%x)'", c);
}

/*
 * See 3GPP TS 23.038:
 *   Individual parameters -> General principles -> Character packing ->
 *   SMS Packing -> Packing of 7-bit characters.
 */
static int
_decode_alphanumeric(struct Buffer *dest, const uint8_t *src, size_t n)
{
	int ret = buffer_putc(dest, '"');
	uint8_t k = 7;
	uint8_t r = 0;

	for (; ret == 0 && n > 0; --n) {
		ret |= buffer_putc(dest, ((*src & ~(-1 << k)) << (7 - k)) | r);
		r = *src >> k;
		++src;

		if (--k == 0) {
			if (r == 0) {
				if (n == 1)
					break;
				else
					return -1;
			}

			ret |= buffer_putc(dest, r);

			k = 7;
			r = 0;
		}
	}

	return (ret == 0 && (r == 0 || buffer_putc(dest, r) == 0))
		? buffer_putc(dest, '"') : -1;
}

/*
 * See 3GPP TS 23.040:
 *   Protocols and protocol architecture -> Protocol element features ->
 *   Numeric and alphanumeric representation -> Address fields;
 */
static int
_check_heading_23040(struct Buffer *dest, const uint8_t *src, size_t n)
{
	if (*src > 2 * (n - 2)) {
		buffer_xprintf(dest, "decode_otherPartySMS: too few bytes"
			       " for given Address-Length (%u)", *src);
		return -1;
	} else if (*src < 2*(n - 2) - 1) {
		buffer_xprintf(dest, "decode_otherPartySMS: too many bytes"
			       " for given Address-Length (%u)", *src);
		return -1;
	}

	if ((src[1] & 0x80) == 0) {
		buffer_xprintf(dest, "decode_otherPartySMS: invalid"
			       " Type-of-Address octet (0x%02x)", src[1]);
		return -1;
	}

	return 0;
}

int
decode_otherPartySMS(struct Buffer *dest, const uint8_t *src, size_t n)
{
	if (n < 2) {
		buffer_xprintf(dest, "decode_otherPartySMS: too few bytes"
			       " (%lu)", (unsigned long) n);
		return -1;
	} else if (n > 12) {
		buffer_xprintf(dest, "decode_otherPartySMS: too many bytes"
			       " (%lu)", (unsigned long) n);
		return -1;
	}

	if (_check_heading_23040(dest, src, n) != 0)
		return -1;

	int rv = 0;
	uint8_t c = (src[1] >> 4) & 7; /* type of number */

	if (c == 5) { /* alphanumeric */
		if ((src[1] & 15) != 0) {
			buffer_xprintf(dest, "decode_otherPartySMS: invalid"
				       " Type-of-Address octet (0x%x)\n"
				       "\tType of number is alphanumeric, but"
				       " numbering plan is not null.", src[1]);
			return -1;
		}
		if (_decode_alphanumeric(dest, src + 2, n - 2) != 0)
			return -1;
	} else if (decode_TBCDstring(dest, src + 2, n - 2) != 0) {
		return -1;
	}

	const char *type_of_number[] = {
		/* b000 */ "unknown",
		/* b001 */ "international",
		/* b010 */ "national",
		/* b011 */ "'network specific'",
		/* b100 */ "subscriber",
		/* b101 */ "alphanumeric",
		/* b110 */ "abbreviated",
		/* b111 */ "reserved"
	};
	rv |= buffer_printf(dest, " type=%s", type_of_number[c]);

	if (c == 5)
		return rv;

	rv |= buffer_printf(dest, " plan=");
	switch (c = src[1] & 15) {
	case 0: return rv | buffer_printf(dest, "unknown");
	case 1: return rv | buffer_printf(dest, "ISDN/telephony");
	case 3: return rv | buffer_printf(dest, "data");
	case 4: return rv | buffer_printf(dest, "telex");
	case 5: return rv | buffer_printf(dest, "'service centre specific 5'");
	case 6: return rv | buffer_printf(dest, "'service centre specific 6'");
	case 8: return rv | buffer_printf(dest, "national");
	case 9: return rv | buffer_printf(dest, "private");
	case 10: return rv | buffer_printf(dest, "ERMES");
	default:;
	}

	return rv | buffer_printf(dest, "'reserved (0x%x)'", c);
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
