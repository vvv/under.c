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

/* Format representation */
struct Format_Repr {
	struct hlist_head *dict; /* dictionary of tags' representations */
	struct hlist_head libs; /* plugins in use */
};
#define FORMAT_REPR(name) struct Format_Repr name = { NULL, HLIST_HEAD_INIT }

/* Read .conf file filling `dest' */
int repr_create(struct Format_Repr *dest, const char *conf_path);

/* Free resources allocated for `fmt' */
void repr_destroy(struct Format_Repr *fmt);

/* Print tag header's representation to stdout */
void repr_show_header(const struct Format_Repr *fmt, enum Tag_Class cls,
		      uint32_t num);

#endif /* _REPR_H */
