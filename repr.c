/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include "repr.h"
#include "list.h"
#include "hash.h"
#include "util.h"

enum { HASH_NBITS = 8 };
#define hashfn(TAGKEY)  hash_long((TAGKEY), HASH_NBITS)

static inline uint32_t
tagkey(enum Tag_Class cls, uint32_t num)
{
	return cls << 30 | num;
}

/* Tag representation attributes */
struct Repr_Attr {
	struct hlist_node n;

	uint32_t key; /* hash key */
	char *name;
	int (*decode)(const struct Pstring *, char *, size_t);
	/* int (*encode)(const struct Pstring *, struct Pstring *); */
};

struct hlist_head *
repr_create_htab(void)
{
	const size_t nbuckets = 1 << HASH_NBITS;
	struct hlist_head *h = xmalloc(nbuckets * sizeof(struct hlist_head));

	size_t i;
	for (i = 0; i < nbuckets; ++i)
		INIT_HLIST_HEAD(&h[i]);

	return h;
}

void
repr_destroy_htab(struct hlist_head *htab)
{
	if (htab == NULL)
		return;

	const size_t nbuckets = 1 << HASH_NBITS;

	struct hlist_node *x, *tmp;
	struct Repr_Attr *attr;
	size_t i;
	for (i = 0; i < nbuckets; ++i) {
		hlist_for_each_safe(x, tmp, &htab[i]) {
			attr = hlist_entry(x, struct Repr_Attr, n);
			free(attr->name);
			free(attr);
		}
		htab[i].first = NULL;
	}

	free(htab);
}

static const struct Repr_Attr *
find_attr(const struct hlist_head *htab, uint32_t key)
{
	assert(htab != NULL);

	struct Repr_Attr *attr;
	struct hlist_node *x;

	hlist_for_each_entry(attr, x, &htab[hashfn(key)], n) {
		if (attr->key == key)
			return attr;
	}

	return NULL;
}

void
repr_show_header(const struct hlist_head *htab, enum Tag_Class cls,
		 uint32_t num)
{
	const struct Repr_Attr *r;

	if (htab == NULL || (r = find_attr(htab, tagkey(cls, num))) == NULL)
		printf("%c%u", "uacp"[cls], num);
	else
		printf(":%s", r->name);
}

#if 1 /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
static struct Repr_Attr *
new_reprattr(uint32_t key, const char *name)
{
	struct Repr_Attr *x = xmalloc(sizeof(struct Repr_Attr));

	INIT_HLIST_NODE(&x->n);
	x->key = key;
	if (asprintf(&x->name, name) < 0)
		die("Out of memory, asprintf failed");
	x->decode = NULL;

	return x;
}

void
repr_fill_htab_XXX(struct hlist_head *htab)
{
	assert(htab != NULL);
	struct Repr_Attr *attr;

	attr = new_reprattr(tagkey(TC_PRIVATE, 7), "servedMsisdn");
	hlist_add_head(&attr->n, &htab[hashfn(attr->key)]);

	attr = new_reprattr(tagkey(TC_PRIVATE, 71), "callTransactionType");
	hlist_add_head(&attr->n, &htab[hashfn(attr->key)]);
}
#endif /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
