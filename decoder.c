/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include "decoder.h"
#include "asn1.h"
#include "util.h"
#include "repr.h"

#ifdef FILLERS
static inline bool
isfiller(uint8_t c)
{
	return c == 0xff;
}
#endif

/*
 * Parse tag identifier and length octets and store decoded attributes
 * in a `*tag'.
 */
static IterV
#ifdef FILLERS
decode_header(struct ASN1_Header *tag, struct Stream *str, bool at_root_p)
#else
decode_header(struct ASN1_Header *tag, struct Stream *str)
#endif
{
	static int cont = 0; /* Position to continue execution from */
	static size_t len_sz; /* Number of length octets, excluding initial */

	uint8_t c;

	switch (cont) {
	case 0:
#ifdef FILLERS
		if (at_root_p && drop_while(isfiller, str) == IE_CONT)
			return IE_CONT;
#endif

		cont = 1;
	case 1: /* Identifier octet(s) -- cases 0..2 */
		if (str->type == S_EOF)
			return IE_DONE;

		if (head(&c, str) == IE_CONT)
			return IE_CONT;

		tag->cls = (c & 0xc0) >> 6;
		tag->cons_p = (c & 0x20) != 0;

		if ((tag->num = c & 0x1f) == 0x1f)
			tag->num = 0; /* tag number > 30 */
		else
			goto tagnum_done;

		cont = 2;
	case 2: /* Tag number > 30 (``high'' tag number) */
		for (; str->size > 0 && *str->data & 0x80;
		     ++str->data, --str->size)
			tag->num = (tag->num << 7) | (*str->data & 0x7f);

		if (head(&c, str) == IE_CONT)
			return IE_CONT;
		tag->num = (tag->num << 7) | (c & 0x7f);

tagnum_done:
		if (tag->num & 0xc0000000) { /* exceeds 30 bits */
			set_error(&str->errmsg, "Tag number is too big: %u",
				  tag->num);
			return IE_CONT;
		}

		cont = 3;
	case 3: /* Initial length octet */
		if (head(&c, str) == IE_CONT)
			return IE_CONT;

		if (c == 0xff) {
			set_error(&str->errmsg, "Length encoding is invalid\n"
				  "  [ITU-T X.690, 8.1.3.5-c]");
			return IE_CONT;
		}

		if (c & 0x80) {
			len_sz = c & 0x7f; /* long form */
			tag->len = 0;
		} else {
			tag->len = c; /* short form*/
			break;
		}

		cont = 4;
	case 4: /* Subsequent length octet(s) */
		for (; str->size > 0 && len_sz > 0;
		     --len_sz, ++str->data, --str->size)
			tag->len = (tag->len << 8) | *str->data;

		if (len_sz > 0)
			return IE_CONT;

		break;
	default:
		assert(0 == 1);
	}

	cont = 0;
	return IE_DONE;
}

/*
 * Print hex dump of a primitive encoding.
 *
 * @enough: Are there enough bytes in `str' to reach the end of tag?
 */
static IterV
print_prim(bool enough, struct Stream *str)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		putchar('"');

		cont = 1;
	case 1:
		{
			uint8_t c;
			if (head(&c, str) == IE_CONT) {
				if (!enough)
					return IE_CONT;
				break; /* ``empty'' tag  (clen == 0) */
			}
			printf("%02x", c);
		}

		cont = 2;
	case 2:
		for (; str->size > 0; ++str->data, --str->size)
			printf(" %02x", *str->data);

		if (!enough)
			return IE_CONT;

		break;
	default:
		assert(0 == 1);
	}

	putchar('"');

	cont = 0;
	return IE_DONE;
}
/* ------------------------------------------------------------------ */

/* Type of elements of `DecSt.caps' list */
struct Capacity {
	struct list_head h;
	size_t value;
};

static inline size_t
capacity(const struct list_head *pos)
{
	return list_entry(pos, struct Capacity, h)->value;
}

/*
 * Remaining capacity -- the number of bytes, available at current
 * level of tag hierarchy.
 */
static inline size_t
remcap(const struct DecSt *z)
{
	assert(z->depth != 0);
	return capacity(z->caps.next);
}

/*
 * Is there enough capacity for that many bytes?
 *
 * @n: number of bytes
 */
static inline bool
contained_p(size_t n, const struct DecSt *z)
{
	return z->depth == 0 /* infinite capacity */ || remcap(z) >= n;
}

static void
add_capacity(size_t n, struct DecSt *dest)
{
	struct Capacity *new = xmalloc(sizeof(struct Capacity));
	new->value = n;

	list_add(&new->h, &dest->caps);
	++dest->depth;
}

static inline void
decrease_capacities(size_t delta, struct DecSt *z)
{
	struct list_head *p;
	__list_for_each(p, &z->caps)
		list_entry(p, struct Capacity, h)->value -= delta;
}

#ifdef DEBUG
static void
check_DecSt_invariant(const struct DecSt *z)
{
	uint32_t len = 0;
	const struct list_head *p;

	__list_for_each(p, &z->caps) {
		assert(list_is_last(p, &z->caps) ||
		       capacity(p) <= capacity(p->next));
		++len;
	}

	assert(len == z->depth);
}
#else
#  define check_DecSt_invariant(...)
#endif

/*
 * Remove "drained off" capacities from `z->caps' list, freeing their
 * memory. Decrease `z->depth' by the number of deleted elements and
 * print that many closing parentheses.
 */
static void
delete_zero_capacities(struct DecSt *z)
{
	struct list_head *p = z->caps.next;
	while (p != &z->caps && capacity(p) == 0) {
		__list_del(p->prev, p->next);

		void *x = list_entry(p, struct Capacity, h);
		p = p->next;
		free(x);

		--z->depth;
		putchar(')');
	}

	check_DecSt_invariant(z);
}

#ifdef DEBUG
static void
debug_show_decoder_state(const struct DecSt *z, const struct Stream *str,
			 const struct Stream *master, const char *format, ...)
{
	fprintf(stderr, "(DEBUG) %lu/%lu d=%u [",
		(unsigned long) str->size, (unsigned long) master->size,
		z->depth);
	if (!list_empty(&z->caps)) {
		fprintf(stderr, "%lu", (unsigned long) remcap(z));

		const struct list_head *p;
		for (p = z->caps.next->next; p != &z->caps; p = p->next)
			fprintf(stderr, ",%lu", (unsigned long) capacity(p));
	}
	fputc(']', stderr);

	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fputc('\n', stderr);
}
#else
#  define debug_show_decoder_state(...)
#endif

IterV
decode(struct DecSt *z, struct Stream *master)
{
	static bool header_p = true; /* Do we parse tag header at this step? */
	static struct ASN1_Header tag;

	if (master->type == S_EOF) {
		if (z->depth == 0) {
			return IE_DONE;
		} else {
			set_error(&master->errmsg, "Unexpected EOF");
			return IE_CONT;
		}
	}

	struct Stream str; /* Substream, passed to an iteratee */
	str.type = master->type;
	str.data = master->data;
	str.errmsg = master->errmsg;

	for (;;) {
		str.size = z->depth == 0 ?
			master->size : MIN(remcap(z), master->size);
		const size_t orig_size = str.size;
		debug_show_decoder_state(z, &str, master, " %s", header_p ?
					 "decode_header" : "print_prim");

		const IterV indic = header_p
#ifdef FILLERS
			? decode_header(&tag, &str, z->depth == 0)
#else
			? decode_header(&tag, &str)
#endif
			: print_prim(remcap(z) <= str.size, &str);
		assert(indic == IE_DONE || indic == IE_CONT);

		decrease_capacities(orig_size - str.size, z);

		master->data = str.data;
		master->size -= orig_size - str.size;
		master->errmsg = str.errmsg;
		debug_show_decoder_state(z, &str, master, " => IE_%s",
					 indic == IE_DONE ? "DONE" : "CONT");

		if (indic == IE_CONT)
			return IE_CONT;

		/* IE_DONE */
		if (header_p) {
			putchar('(');
			repr_show_header(z->repr_htab, tag.cls, tag.num);

			if (tag.len == 0) {
				fputs(tag.cons_p ? " ()" : " \"\"", stdout);
				add_capacity(0, z);
			}

			delete_zero_capacities(z);

			if (tag.len == 0)
				goto line_feed;

			if (!contained_p(tag.len, z)) {
				set_error(&master->errmsg,
					  "Tag is too big for its container");
				return IE_CONT;
			}
			add_capacity(tag.len, z);

			if (!tag.cons_p) {
				header_p = false;
				putchar(' ');
				continue;
			}
		} else {
			delete_zero_capacities(z);
			header_p = true;
		}

line_feed:
		putchar('\n');
		uint32_t i;
		for (i = 0; i < z->depth; ++i)
			fputs("    ", stdout);
	}

	assert(0 == 1);
	return -1; /* never reached */
}
