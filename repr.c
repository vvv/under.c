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
#include <regex.h>
#include <ctype.h>

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

static const char *
plugin_name(char *path)
{
	char *fn = basename(path);
	char *p = strrchr(fn, '.');

	if (p == NULL || !streq(p, ".conf")) {
		fputs("-f/--format parameter must have `.conf' suffix\n",
		      stderr);
		return NULL;
	}
	*p = 0;

	return fn;
}

static void
add_repr(struct hlist_head *htab, const char *spec, const char *name,
	 const char *plugin, const char *codec)
{
	debug_print("add_repr: tag=%s name=%s plugin=lib%s.so codec=%s\n",
		    spec, name, plugin, codec);

	enum Tag_Class cls;
	unsigned long num;

	switch (*spec) {
	case 'u': cls = TC_UNIVERSAL; break;
	case 'a': cls = TC_APPLICATION; break;
	case 'c': cls = TC_CONTEXT; break;
	case 'p': cls = TC_PRIVATE; break;
	default:
		assert(0 == 1);
	}

	errno = 0;
	num = strtoul(spec + 1, NULL, 10);
	assert(errno == 0);

	struct Repr_Attr *attr = xmalloc(sizeof(struct Repr_Attr));
	INIT_HLIST_NODE(&attr->n);
	attr->key = tagkey(cls, num);
	if (asprintf(&attr->name, "%s", name) < 0)
		die("Out of memory, asprintf failed");
	attr->decode = NULL; /* XXX */

	hlist_add_head(&attr->n, &htab[hashfn(attr->key)]);
}

static int
parse_conf(struct hlist_head *dest, FILE *f, const char *default_plugin,
	   const char *path)
{
	regex_t re_rstrip; /* trailing whitespace and/or comments */
	assert(regcomp(&re_rstrip, "[ \t]*(#.*)?$", REG_EXTENDED) == 0);

	regex_t re_entry; /* tag configuration entry */
#define ID "[a-zA-Z][a-zA-Z0-9_-]*"
	assert(regcomp(&re_entry, "^([uacp][0-9]+)"
		       "[ \t]+(" ID ")"
		       "([ \t]+(" ID "\\.)?(" ID ")?)?"
		       "\n?$", REG_EXTENDED) == 0);
#undef ID
	regmatch_t groups[re_entry.re_nsub + 1];

	char *line = NULL;
	size_t sz = 0;
	size_t ln; /* line number */
	char *p;
	int retval = -1;

	for (ln = 1; getline(&line, &sz, f) >= 0; ++ln) {
		if (regexec(&re_rstrip, line, 1, groups, 0) == 0)
			line[groups->rm_so] = 0; /* skip trailing junk */

		for (p = line; isspace(*p); ++p)
			; /* skip leading whitespace */

		if (*p == 0)
			continue; /* empty line */

		if (regexec(&re_entry, p, re_entry.re_nsub + 1, groups, 0)
		    != 0) {
			fprintf(stderr, "%s:%lu: Syntax error\n", path,
				(unsigned long) ln);
			goto end;
		}

		p[groups[1].rm_eo] = p[groups[2].rm_eo] = 0;

		if (groups[3].rm_so == -1) { /* no "plugin.codec" group */
			add_repr(dest, p + groups[1].rm_so, p + groups[2].rm_so,
				 default_plugin, p + groups[2].rm_so);
			continue;
		}

		if (groups[4].rm_so != -1)
			p[groups[4].rm_eo - 1] = 0;

		if (groups[5].rm_so != -1)
			p[groups[5].rm_eo] = 0;

		/* XXX shouldn't we return anything? */
		add_repr(dest, p + groups[1].rm_so, p + groups[2].rm_so,
			 groups[4].rm_so == -1
			 ? default_plugin : p + groups[4].rm_so,
			 p + groups[groups[5].rm_so == -1 ? 2 : 5].rm_so);		}

	retval = 0;
end:
	free(line);
	regfree(&re_entry);
	regfree(&re_rstrip);

	return retval;
}

int
repr_read_conf(struct hlist_head *dest, const char *path)
{
	debug_print("repr_read_conf: `%s'", path);

	char *__path = strdup(path);
	if (__path == NULL)
		die("Out of memory, strdup failed");

	int retval = -1;
	FILE *f = NULL;

	const char *default_plugin = plugin_name(__path);
	if (default_plugin == NULL)
		goto end;

	if ((f = fopen(path, "r")) == NULL) {
		error(0, errno, "%s", path);
		goto end;
	}

	retval = parse_conf(dest, f, default_plugin, path);
end:
	if (f != NULL)
		fclose(f);
	free(__path);

	return retval;
}

#if 0 /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
static struct Repr_Attr *
new_reprattr(uint32_t key, const char *name)
{
	struct Repr_Attr *x = xmalloc(sizeof(struct Repr_Attr));

	INIT_HLIST_NODE(&x->n);
	x->key = key;
	if (asprintf(&x->name, "%s", name) < 0)
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
