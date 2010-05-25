#include <assert.h>
#include "codec.h"
#include "decoder.h"
#include "encoder.h"

IterV
run_codec(enum Codec_T type, void **state, struct Stream *str)
{
	if (type == DECODER) {
		if (*state == NULL) {
			*state = xmalloc(sizeof(struct DecSt));
			init_DecSt(*state);
		}
		return decode(*state, str);
	} else if (type == ENCODER) {
		if (*state == NULL) {
			*state = xmalloc(sizeof(struct EncSt));
			init_EncSt(*state);
		}
		return encode(*state, str);
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
