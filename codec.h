#ifndef _CODEC_H
#define _CODEC_H

#include "iteratee.h"

/* Type of codec */
enum Codec_T { DECODER, ENCODER };

/* XXX */
IterV run_codec(enum Codec_T type, void **z, struct Stream *str);

/* Release resources allocated for codec's state (z) */
void free_codec(enum Codec_T type, void *z);

#endif /* _CODEC_H */
