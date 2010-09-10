/*
 * $ cd plugins
 * $ ln -s libunder_sr.so.0 libunder_sr.so
 * $ gcc -g -Wall -W -I.. -L. -lunder_sr -Wl,-rpath,`pwd` ../tests/test_sr.c \
 *       -o /tmp/1
 */
#include <assert.h>
#include <stdio.h>
#include "util.h"

extern int decode_callTransactionType(const struct Pstring *src,
				      char *dest, size_t n);
extern int _ctt_tonum(const char *src, uint8_t *dest);

int main(void)
{
	uint8_t s[] = { 27, 0 };
	struct Pstring src = { 2, s };
	char dest[64] = {0};

	assert(decode_callTransactionType(&src, dest, sizeof(dest)) == -1);
	assert(streq(dest, "decode_callTransactionType: 1 byte expected,"
		     " 2 received"));

	src.size = 1;
	assert(decode_callTransactionType(&src, dest, sizeof(dest)) == 0);
	assert(streq(dest, "transit (27)"));

	*s = 3;
	assert(decode_callTransactionType(&src, dest, sizeof(dest)) == 0);
	assert(streq(dest, "emergencyCall (3)"));

	*s = 0;
	assert(decode_callTransactionType(&src, dest, sizeof(dest)) == 0);
	assert(streq(dest, "default (0)"));

	*s = 125;
	assert(decode_callTransactionType(&src, dest, sizeof(dest)) == 0);
	assert(streq(dest, "ussdCall (125)"));

	*s = 126;
	assert(decode_callTransactionType(&src, dest, sizeof(dest)) == -1);
	assert(streq(dest, "decode_callTransactionType: unsupported value"
		     " (126)"));

	uint8_t n = 0;
	assert(_ctt_tonum("transit", &n) == 0);
	assert(n == 27);
	assert(_ctt_tonum("moLocationRequest", &n) == 0);
	assert(n == 37);
	assert(_ctt_tonum("moLocationRequestAttempt", &n) == 0);
	assert(n == 38);
	assert(_ctt_tonum("unknown", &n) == -1);
	assert(n == 38);
	assert(_ctt_tonum("callForwarding", &n) == 0);
	assert(n == 29);
	assert(_ctt_tonum("voiceGroupServiceRMSC", &n) == 0);
	assert(n == 46);

	return 0;
}
