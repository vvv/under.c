/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _REPR_H
#define _REPR_H

#include "asn1.h"

/* XXX TODO documentation */
struct hlist_head * repr_create_htab(void);
void repr_destroy_htab(struct hlist_head *htab);
int repr_read_conf(struct hlist_head *dest, const char *path);

/* Print tag header's representation to stdout */
void repr_show_header(const struct hlist_head *htab, enum Tag_Class cls,
		      uint32_t num);

#endif /* _REPR_H */
