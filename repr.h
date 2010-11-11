/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _REPR_H
#define _REPR_H

#include "list.h"
#include "asn1.h"

struct Buffer;

/*
 * Format specification.
 *
 * This structure "knows" how to convert tag data to human-friendly
 * representations and vice versa.
 */
struct Repr_Format {
	struct hlist_head *dict; /* Dictionary of tags' representations */
	struct hlist_head libs; /* Plugins in use */
};
#define REPR_FORMAT(name) struct Repr_Format name = { NULL, HLIST_HEAD_INIT }

/* Read configuration file and fill `dest' */
int repr_create(struct Repr_Format *dest, const char *conf_path);

/* Free resources allocated for `fmt' */
void repr_destroy(struct Repr_Format *fmt);

/* Print tag header's representation to stdout */
void repr_show_header(const struct Repr_Format *fmt, enum Tag_Class cls,
		      uint32_t num);

/*
 * Repr_Codec -- type of function that converts raw bytes to
 * human-friendly representation or vice versa.
 *
 * @dest: destination buffer
 * @src: start of raw data
 * @n: number of bytes
 *
 * Return value: 0 - success, -1 - conversion failed or insufficient
 * `dest' capacity.
 *
 * Codecs should write error message to `dest' in case of error.
 */
typedef int (*Repr_Codec)(struct Buffer *dest, const uint8_t *src, size_t n);

/*
 * Return tag value decoder -- converter from a primitive encoding to
 * human-friendly representation.
 *
 * Returned value can be NULL.
 */
Repr_Codec repr_from_raw(const struct Repr_Format *fmt, enum Tag_Class cls,
			 uint32_t num);

#endif /* _REPR_H */
