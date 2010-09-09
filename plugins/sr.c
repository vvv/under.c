#include <stdint.h>
#include <stdio.h>

#include "ctt.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int
decode_callTransactionType(const uint8_t *bytes, size_t len)
{
	if (len != 1) {
		fprintf(stderr, "decode_callTransactionType: 1 byte expected,"
			" %lu received\n", (unsigned long) len);
		return -1;
	}

	if (*bytes >= ARRAY_SIZE(ctt_idx) || ctt_idx[*bytes] == 0)
		return -1;

	printf("[%s (%u)]\n", ctt_dict[ctt_idx[*bytes]].symbol, *bytes);
	return 0;
}

/* XXX */
/* encode_callTransactionType(XXX) */
/* { */
/* } */
