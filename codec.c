#include <assert.h>
#include "codec.h"
#include "decoder.h"
#include "encoder.h"

IterV
run_codec(enum Codec_T type, void **z, struct Stream *str)
{
	if (type == DECODER) {
		if (*z == NULL) {
			*z = xmalloc(sizeof(struct DecSt));
			init_DecSt(*z);
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
