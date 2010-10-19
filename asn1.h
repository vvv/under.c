/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _ASN1_H
#define _ASN1_H

#include <stdint.h>
#include <stdbool.h>

enum Tag_Class { TC_UNIVERSAL, TC_APPLICATION, TC_CONTEXT, TC_PRIVATE };

/*
 * Attributes of ASN.1 tag.
 *
 * These values are encoded in tag's identifier and length octets.
 */
struct ASN1_Header {
	enum Tag_Class cls; /* Tag class */
	uint32_t num; /* Tag number */
	bool cons_p; /* Is encoding constructed? */
	size_t len; /* Length of contents */
};

#endif /* _ASN1_H */
