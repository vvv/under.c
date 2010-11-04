/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <libgen.h>
#include <assert.h>
#include <regex.h>
#include <ctype.h>
#include <dlfcn.h>

#include "repr.h"
#include "hash.h"
#include "util.h"

enum { HASH_NBITS = 8 };

/* Tag representation */
struct Repr {
	struct hlist_node _node;

	uint32_t key; /* Hash key*/
	char *name; /* Human-friendly tag name (e.g., "recordType") */

	/* Converters: */
	Repr_Codec decode; /* Raw bytes to human-friendly representation */
	/* Repr_Codec encode; /\* Representation to raw bytes *\/ */
};

struct Plugin {
	struct hlist_node _node;

	char *name; /* name of plugin (library filename = libunder_NAME.so) */
	void *handle; /* opaque handle for the dynamic library */
};

#define NOLIB_HANDLE (void *) 1

void
repr_destroy(struct Repr_Format *fmt)
{
	struct hlist_node *x, *tmp;
	struct Plugin *p;
	hlist_for_each_entry_safe(p, x, tmp, &fmt->libs, _node) {
		free(p->name);
		if (p->handle != NULL && p->handle != NOLIB_HANDLE)
			dlclose(p->handle);
		free(p);
	}
	fmt->libs.first = NULL;

	if (fmt->dict == NULL)
		return;

	const size_t nbuckets = 1 << HASH_NBITS;
	struct Repr *r;

	size_t i;
	for (i = 0; i < nbuckets; ++i) {
		hlist_for_each_entry_safe(r, x, tmp, fmt->dict + i, _node) {
			free(r->name);
			free(r);
		}
		fmt->dict[i].first = NULL;
	}

	free(fmt->dict);
	fmt->dict = NULL;
}

static struct hlist_head *
htab_create(void)
{
	const size_t nbuckets = 1 << HASH_NBITS;
	struct hlist_head *h = xmalloc(nbuckets * sizeof(struct hlist_head));

	size_t i;
	for (i = 0; i < nbuckets; ++i)
		INIT_HLIST_HEAD(h + i);

	return h;
}

/*
 * Return filename component of `path' with file extension stripped.
 * Return NULL if file extension is not equal to `filext'.
 */
static const char *
basename_stripext(char *path, const char *filext)
{
        char *fn = basename(path);
	char *p = fn + strlen(fn) - strlen(filext);

	if (p <= fn || !streq(p, filext))
		return NULL;
        *p = 0;

        return fn;
}

static struct Plugin *
find_plugin(struct hlist_head *libs, const char *name)
{
	if (name == NULL)
		return hlist_entry(libs->first, struct Plugin, _node);

	struct hlist_node *last = libs->first;
	struct hlist_node *x = last->next;
	struct Plugin *lib;

	hlist_for_each_entry_from(lib, x, _node) {
		if (streq(lib->name, name))
			return lib;
		last = x;
	}

	lib = new_zeroed(struct Plugin);
	xasprintf(&lib->name, "%s", name);

	hlist_add_after(last, &lib->_node);
	return lib;
}

static void *
find_symbol(struct hlist_head *libs, const char *plugin, const char *symbol)
{
	debug_print("find_symbol: %s.%s", plugin, symbol);

	struct Plugin *lib = find_plugin(libs, plugin);

	if (lib->handle == NULL) {
                char filename[64] = {0};
                assert((size_t) snprintf(filename, sizeof(filename),
					 "libunder_%s.so", lib->name)
		       < sizeof(filename));

                if ((lib->handle = dlopen(filename, RTLD_LAZY)) == NULL) {
                        fprintf(stderr, "*WARNING* %s\n", dlerror());
			lib->handle = NOLIB_HANDLE;
                        return NULL;
                }
	} else if (lib->handle == NOLIB_HANDLE) {
		return NULL;
	}

        dlerror(); /* clear existing error */
        void *sym = dlsym(lib->handle, symbol);

        const char *err = dlerror();
        if (err == NULL)
		return sym;

	fprintf(stderr, "*WARNING* %s\n", err);
        return NULL;
}

static inline uint32_t
tagkey(enum Tag_Class cls, uint32_t num)
{
        return cls << 30 | num;
}

static uint32_t
tagspec2key(const char *s)
{
	enum Tag_Class cls;
	unsigned long num;

	switch (*s) {
	case 'u': cls = TC_UNIVERSAL; break;
        case 'a': cls = TC_APPLICATION; break;
        case 'c': cls = TC_CONTEXT; break;
        case 'p': cls = TC_PRIVATE; break;
        default:
                assert(0 == 1);
	}

        errno = 0;
        num = strtoul(s + 1, NULL, 10);
        assert(errno == 0);

        return tagkey(cls, num);
}

static const struct Repr *
bucket_getitem(const struct hlist_head *bucket, uint32_t key)
{
        const struct hlist_node *x;
        const struct Repr *r;

        hlist_for_each_entry(r, x, bucket, _node) {
                if (r->key == key)
                        return r;
        }

        return NULL;
}

static int
add_repr(struct Repr_Format *fmt, const char *tag, const char *name,
	 const char *plugin, const char *codec, const char *conf_path)
{
#ifdef DEBUG
	fprintf(stderr, "(DEBUG) add_repr: tag=%s name=%s", tag, name);
	if (codec != NULL)
		fprintf(stderr, " plugin=%s codec=%s", plugin, codec);
	fputc('\n', stderr);
#endif

	struct Repr *r = new_zeroed(struct Repr);
	r->key = tagspec2key(tag);

	if (fmt->dict == NULL)
		fmt->dict = htab_create();

	struct hlist_head *head = fmt->dict + hash_long(r->key, HASH_NBITS);
	if (bucket_getitem(head, r->key) != NULL) {
		fprintf(stderr, "%s: Too many `%s' entries\n", conf_path, tag);
		free(r);
		return -1;
	}

	xasprintf(&r->name, "%s", name);

	if (codec != NULL) {
		const bool defplug_p = plugin == NULL ||
			streq(plugin, hlist_entry(fmt->libs.first,
						  struct Plugin, _node)->name);
		char s[64] = {0};

		r->decode = find_symbol
			(&fmt->libs, defplug_p ? NULL : plugin,
			 strncat(strncpy(s, "decode_", sizeof(s)-1),
				 codec, sizeof(s) - strlen(s) - 1));

		/* XXX r->encode */
	}

	hlist_add_head(&r->_node, head);
	return 0;
}

#ifdef DEBUG
static void
debug_show_format(const struct Repr_Format *fmt)
{
	const struct hlist_node *x;

	if (fmt->dict == NULL) {
		debug_print("repr_dict: NULL");
	} else {
		const unsigned long nbuckets = 1 << HASH_NBITS;
		const struct Repr *r;
		unsigned long i;

		for (i = 0; i < nbuckets; ++i) {
			fprintf(stderr, "(DEBUG) repr_dict[%lu]:", i);
			hlist_for_each_entry(r, x, fmt->dict + i, _node)
				fprintf(stderr, " %c%u", "uacp"[r->key >> 30],
					r->key & 0x3fffffff);
			fputc('\n', stderr);
		}
	}

	fputs("(DEBUG) repr_libs:", stderr);
	const struct Plugin *p;
	hlist_for_each_entry(p, x, &fmt->libs, _node)
		fprintf(stderr, " %s", p->name);
	fputc('\n', stderr);
}
#else
#  define debug_show_format(...)
#endif

static int
parse_conf(struct Repr_Format *dest, FILE *f, const char *conf_path)
{
	regex_t re_rstrip; /* trailing whitespace and/or comments */
	assert(regcomp(&re_rstrip, "[ \t]*(#.*)?$", REG_EXTENDED) == 0);

	regex_t re_entry; /* tag representation entry */
#define ID "[a-zA-Z][a-zA-Z0-9_]*"
        assert(regcomp(&re_entry, "^([uacp][0-9]+)"
                       "[ \t]+(" ID ")"
                       "([ \t]+(" ID "\\.)?(" ID "))?"
                       "\n?$", REG_EXTENDED) == 0);
#undef ID
        regmatch_t groups[re_entry.re_nsub + 1];

	char *line = NULL;
	size_t sz = 0;
	unsigned long ln; /* line number */
	char *p;
	int retval = -1;

	for (ln = 1; getline(&line, &sz, f) >= 0; ++ln) {
		if (regexec(&re_rstrip, line, 1, groups, 0) == 0)
			line[groups->rm_so] = 0; /* delete trailing junk */

		for (p = line; isspace(*p); ++p)
			; /* skip leading whitespace */
		if (*p == 0)
			continue; /* empty line */

		if (regexec(&re_entry, p, re_entry.re_nsub + 1, groups, 0)
		    != 0) {
			fprintf(stderr, "%s:%lu: Syntax error\n", conf_path,
				ln);
			goto end;
		}
		p[groups[1].rm_eo] = p[groups[2].rm_eo] = 0;

		if (groups[3].rm_so == -1) {
			if (add_repr(dest, p + groups[1].rm_so,
				     p + groups[2].rm_so, NULL, NULL,
				     conf_path) == 0)
				continue;
			else
				goto end;
		}

		if (groups[4].rm_so != -1)
			p[groups[4].rm_eo - 1] = 0;
		p[groups[5].rm_eo] = 0;

		if (add_repr(dest, p + groups[1].rm_so, p + groups[2].rm_so,
			     groups[4].rm_so == -1 ? NULL : p + groups[4].rm_so,
                             p + groups[5].rm_so, conf_path) != 0)
			goto end;
	}

	debug_show_format(dest);
	retval = 0;
end:
	free(line);
	regfree(&re_entry);
	regfree(&re_rstrip);

	return retval;
}

static int
check_format_argument(struct hlist_head *libs, const char *conf_path)
{
	assert(hlist_empty(libs));

	char *path = strdup(conf_path);
	if (path == NULL)
		die("Out of memory, strdup failed");

	const char *defplug = basename_stripext(path, ".conf"); /*XXX*/
	if (defplug == NULL) {
		fputs("--format argument must have `.conf' suffix\n", stderr);
		free(path);
		return -1;
	}

	struct Plugin *p = new_zeroed(struct Plugin);
	xasprintf(&p->name, "%s", defplug);
	hlist_add_head(&p->_node, libs);

	free(path);
	return 0;
}

int
repr_create(struct Repr_Format *dest, const char *conf_path)
{
	debug_print("repr_create: `%s'", conf_path);

	if (check_format_argument(&dest->libs, conf_path) != 0)
		return -1;

	FILE *f = fopen(conf_path, "r");
	if (f == NULL) {
		error(0, errno, "%s", conf_path);
		return -1;
	}

	assert(dest->dict == NULL);
	const int rv = parse_conf(dest, f, conf_path);

	fclose(f);
	return rv;
}

static inline const struct Repr *
htab_getitem(const struct hlist_head *htab, uint32_t key)
{
        return htab == NULL ? NULL
		: bucket_getitem(htab + hash_long(key, HASH_NBITS), key);
}

void
repr_show_header(const struct Repr_Format *fmt, enum Tag_Class cls,
		 uint32_t num)
{
	const struct Repr *r = htab_getitem(fmt->dict, tagkey(cls, num));
	if (r == NULL)
		printf("%c%u", "uacp"[cls], num);
	else
		printf(":%s", r->name);
}

Repr_Codec
repr_from_raw(const struct Repr_Format *fmt, enum Tag_Class cls, uint32_t num)
{
	const struct Repr *r = htab_getitem(fmt->dict, tagkey(cls, num));
	return r == NULL ? NULL : r->decode;
}
