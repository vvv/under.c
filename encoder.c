/*
 * Copyright (C) 2010  Valery V. Vorotyntsev <valery.vv@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "encoder.h"
#include "asn1.h"
#include "util.h"

#ifndef _BSD_SOURCE
#  define _BSD_SOURCE
#endif
#include <endian.h>
#ifndef htobe64
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  include <byteswap.h>
#  define htobe64(x) __bswap_64 (x)
# else
#  define htobe64(x) (x)
# endif
#endif

void
init_EncSt(struct EncSt *z)
{
	INIT_BUFFER(&z->acc);
	buffer_resize(&z->acc, 1024);
	INIT_LIST_HEAD(&z->bt);
}

void
free_EncSt(struct EncSt *z)
{
	if (z == NULL)
		return;

	free(buffer_data(&z->acc));
	free(z);
}

static inline bool
_isspace(uint8_t c)
{
	return isspace(c);
}

/* Parse '\s*\(' regexp */
static IterV
left_bracket(struct Stream *str)
{
	if (drop_while(_isspace, str) == IE_CONT)
		return IE_CONT;

	if (*str->data != '(') {
		set_error(str, "`(' expected");
		return IE_CONT;
	}

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '\s*[)(]' regexp, saving met bracket in `*c' */
static IterV
any_bracket(uint8_t *c, struct Stream *str)
{
        if (drop_while(_isspace, str) == IE_CONT)
                return IE_CONT;

        if (*str->data != ')' && *str->data != '(') {
		set_error(str, "`)' or `(' expected");
		return IE_CONT;
	}

	*c = *str->data;

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '(' or '"' character */
static IterV
contents_type(bool *consp, struct Stream *str)
{
	assert(str->size > 0); /* not a kosher iteratee */

	if (*str->data == '(') {
		*consp = true;
	} else if (*str->data == '"') {
		*consp = false;
	} else {
		set_error(str, "`(' or `\"' expected");
		return IE_CONT;
	}

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '\s*[uacp)]' regexp */
static IterV
read_tag_class(enum Tag_Class *dest, bool *nil, struct Stream *str)
{
	if (drop_while(_isspace, str) == IE_CONT)
		return IE_CONT;

	switch (*str->data) {
	case 'u': *dest = TC_UNIVERSAL; break;
	case 'a': *dest = TC_APPLICATION; break;
	case 'c': *dest = TC_CONTEXT; break;
	case 'p': *dest = TC_PRIVATE; break;
	case ')': *nil = true; break;
	default:
		set_error(str, "Invalid tag class specification");
		return IE_CONT;
	}

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '[0-9]+\s' regexp */
static IterV
read_tag_number(uint32_t *dest, struct Stream *str)
{
	static uint32_t n = 0; /* number of parsed digits */

	for (; str->size > 0 && isdigit(*str->data);
	     ++str->data, --str->size) {
		if (++n > 10) {
			set_error(str, "Invalid tag number: too many digits");
			return IE_CONT;
		}

		*dest = 10*(*dest) + (*str->data - '0');
	}

	if (str->size == 0)
		return IE_CONT;

	if (n == 0) {
		set_error(str, "Digit expected");
		return IE_CONT;
	}
	n = 0;

	if (!isspace(*str->data)) {
		set_error(str, "White-space character expected");
		return IE_CONT;
	}
	++str->data;
	--str->size;

	if (*dest & 0xc0000000) { /* tagnum exceeds 30 bits */
		set_error(str, "Enormous tag number", *dest);
		return IE_CONT;
	}

	return IE_DONE;
}

/* Parse '\s*([uacp][0-9]+\s+|\))' regexp */
IterV
read_header(struct ASN1_Header *tag, bool *nil, struct Stream *str)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		if (read_tag_class(&tag->cls, nil, str) == IE_CONT)
			return IE_CONT;
		if (*nil)
			break;

		tag->num = 0;
		++cont;
	case 1:
		if (read_tag_number(&tag->num, str) == IE_CONT)
			return IE_CONT;

		++cont;
	case 2:
		if (drop_while(_isspace, str) == IE_CONT)
			return IE_CONT;

		break;
	default:
		assert(0 == 1);
	}

	cont = 0;
	return IE_DONE;
}

static int
store(struct Buffer *dest, const void *src, size_t n, struct Stream *str)
{
	int r;
	if ((r = buffer_put(dest, src, n)) != 0)
		set_error(str, "Insufficient capacity of encoded bytes'"
			  " accumulator");
	return r;
}

static int
store1(struct Buffer *dest, uint8_t c, struct Stream *str)
{
	int r;
	if ((r = buffer_putc(dest, c)) != 0)
		set_error(str, "Insufficient capacity of encoded bytes'"
			  " accumulator");
	return r;
}

/* Read-only memory region */
struct Pstring {
	const uint8_t *data;
	size_t size;
};

/* Parse '\s*([0-9a-fA-F]{2}(\s+[0-9a-fA-F]{2})*\s*)?"' regexp */
static IterV
primval(struct Pstring *dest, struct Buffer *acc, struct Stream *str)
{
	static uint8_t nibble = 0;
	static bool expect_space = false;

	uint8_t c;
	for (;;) {
		if (head(&c, str) == IE_CONT)
			return IE_CONT;

		if (nibble == 0) {
			if (c == '"') {
				expect_space = false;
				return IE_DONE;
			}

			if (isspace(c)) {
				expect_space = false;
				if (drop_while(_isspace, str) == IE_CONT)
					return IE_CONT;

				c = *str->data;
				++str->data;
				--str->size;

				if (c == '"')
					return IE_DONE;
			} else if (expect_space) {
				set_error(str, "White-space character"
					  " expected");
				return IE_CONT;
			}
		}

		if (!isxdigit(c)) {
			set_error(str, "Hexadecimal digit expected");
			return IE_CONT;
		}

		if (nibble == 0) {
			nibble = c;
			continue;
		} else {
			const char s[] = { nibble, c, 0 };
			if (store1(acc, strtoul(s, NULL, 16), str) != 0)
				return IE_CONT;
			++dest->size;

			nibble = 0;
			expect_space = true;
		}
	}
}

/*
 * Read a primitive value, updating `*dest' p-string.
 * Parse '\s*([0-9a-fA-F]{2}(\s+[0-9a-fA-F]{2})*\s*)?"\s*\)' regexp.
 */
static IterV
read_primitive(struct Pstring *dest, struct EncSt *z, struct Stream *str)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		dest->data = z->acc.wptr;
		dest->size = 0;

		++cont;
	case 1:
		if (primval(dest, &z->acc, str) == IE_CONT)
			return IE_CONT;

		++cont;
	case 2:
		if (drop_while(_isspace, str) == IE_CONT)
			return IE_CONT;

		if (*str->data != ')') {
			set_error(str, "`)' expected");
			return IE_CONT;
		}
		++str->data;
		--str->size;

		break;
	default:
		assert(0 == 1);
		return -1;
	}

	debug_print("read_primitive: %lu bytes encoded",
		    (unsigned long) dest->size);
	cont = 0;
	return IE_DONE;
}

/*
 * Append encoding of "high" tag number to accumulator.
 * Note, that the first argument is expected to be greater than 30.
 */
static int
encode_htagnum(struct Buffer *acc, uint32_t val, struct Stream *str)
{
	uint8_t buf[sizeof(val)*8/7 + 1];
	register uint8_t *p = buf + sizeof(buf);

	do {
		*(--p) = 0x80 | (val & 0x7f);
		val >>= 7;
	} while (val != 0);

	buf[sizeof(buf) - 1] &= 0x7f;

	return store(acc, p, buf + sizeof(buf) - p, str);
}

/*
 * Append encoding of "long" length to accumulator.
 *
 * @acc: encoding bytes' accumulator
 * @val: the length value to encode
 * @str: where to put error messages
 *
 * Note, that the first argument is expected to be greater than 0x7f.
 */
static int
encode_longlen(struct Buffer *acc, size_t val, struct Stream *str)
{
	const uint64_t ben = htobe64(val);
	const uint8_t *p = (void *) &ben;
	const uint8_t *end = p + sizeof(ben);

	while (*p == 0 && p < end)
		++p;

	return (store1(acc, 0x80 | (end - p), str) == 0 &&
		store(acc, p, end - p, str) == 0) ?
		0 : -1;
}

union U_Header {
	struct ASN1_Header rec; /* Intermediate representation */
	struct Pstring enc; /* DER encoding */
};

/* Encode tag header and write the encoding to accumulator */
static int
encode_header(union U_Header *io, struct Buffer *acc, struct Stream *str)
{
	struct Pstring encoding = { acc->wptr, 0 };

	const struct ASN1_Header *h = &io->rec;
	debug_print("encode_header: %c%u %s %lu \\", "uacp"[h->cls], h->num,
		    h->cons_p ? "cons" : "prim", (unsigned long) h->len);

	if (store1(acc, (h->cls << 6) | (h->cons_p ? 0x20 : 0) |
		    (h->num <= 30 ? h->num : 0x1f), str) != 0)
		return -1;
	++encoding.size;

	if (h->num > 30) { /* high tag number */
		const size_t orig_size = acc->size;
		if (encode_htagnum(acc, h->num, str) != 0)
			return -1;
		encoding.size += orig_size - acc->size;
	}

	if (h->len < 0x80) { /* short length */
		if (store1(acc, h->len, str) != 0)
			return -1;
		++encoding.size;
	} else { /* long length */
		const size_t orig_size = acc->size;
		if (encode_longlen(acc, h->len, str) != 0)
			return -1;
		encoding.size += orig_size - acc->size;
	}

	debug_hexdump("encode_header: \\", encoding.data, encoding.size);
	io->enc.data = encoding.data;
	io->enc.size = encoding.size;

	return 0;
}

/* Node of the ``encoding tree'' */
struct Node {
	struct Node *next, /* Next sibling; NULL for the last node */
		*child; /* First child of this node; NULL for a leaf node */

	union U_Header header; /* Tag header (i.e., identifier and length) */

	/* DER encoding of primitive contents; NULL for constructed tags */
	struct Pstring *contents;
};

/* Backtrace element */
struct Frame {
	struct list_head h;
	struct Node *node; /* The encoding tree's node this frame points to */
};

/* Return the current node; return NULL if the backtrace is empty */
static inline struct Node *
curnode(const struct EncSt *z)
{
	return list_empty(&z->bt) ? NULL :
		list_first_entry(&z->bt, struct Frame, h)->node;
}

/*
 * Return the parent of the current node.
 * Note, that the backtrace is expected to contain at least two frames.
 */
static inline struct Node *
_parent(const struct EncSt *z)
{
	assert(!list_empty(&z->bt));
	assert(!list_is_last(z->bt.next, &z->bt));
	return list_entry(z->bt.next->next, struct Frame, h)->node;
}

/*
 * Address of parent's `.header.rec.len' member.
 *
 * Note, that the backtrace is expected to contain at least two frames.
 * And you better be sure that the parent's header is not encoded yet.
 */
static inline size_t *
parent_len(const struct EncSt *z)
{
	return &_parent(z)->header.rec.len;
}

/*
 * Test whether there is exactly one frame in the backtrace.
 * Note, that `z->bt' list is expected to be not empty.
 */
static inline bool
at_root_frame(const struct EncSt *z)
{
	return list_is_last(z->bt.next, &z->bt);
}

static void
push_frame(struct Node *target, struct EncSt *dest)
{
	struct Frame *new = xmalloc(sizeof(struct Frame));
	INIT_LIST_HEAD(&new->h);
	new->node = target;

	list_add(&new->h, &dest->bt);
}

static void
pop_frame(struct EncSt *z)
{
	assert(!list_empty(&z->bt));

	struct list_head *p = z->bt.next;
	__list_del(p->prev, p->next);
	free(p);
}

IterV
read_tree(struct EncSt *z, struct Stream *str)
{
	assert(str->type == S_CHUNK);
	static int cont = 0;

	struct Node *cur = curnode(z);
	bool nil; /* true for empty values -- `()', false otherwise */

	switch (cont) {
	case 0:
		assert(list_empty(&z->bt));

		if (left_bracket(str) == IE_CONT)
			return IE_CONT;

		cur = new_zeroed(struct Node);
		push_frame(cur, z);

header:
		++cont;
	case 1:
		nil = false;
		if (read_header(&cur->header.rec, &nil, str) == IE_CONT)
			return IE_CONT;

		if (nil) {
			if (at_root_frame(z)) {
				pop_frame(z);
				break;
			} else {
				goto tag_end;
			}
		}

		++cont;
	case 2:
		if (contents_type(&cur->header.rec.cons_p, str) == IE_CONT)
			return IE_CONT;

		if (cur->header.rec.cons_p) {
			cur = cur->child = new_zeroed(struct Node);
			push_frame(cur, z);

			goto header;
		}

		cur->contents = new_zeroed(struct Pstring);

		++cont;
	case 3:
		if (read_primitive(cur->contents, z, str) == IE_CONT)
			return IE_CONT;
		cur->header.rec.len = cur->contents->size;

		if (at_root_frame(z))
			break;

		if (encode_header(&cur->header, &z->acc, str) != 0)
			return IE_CONT; /* error */
		*parent_len(z) += cur->header.enc.size + cur->contents->size;

tag_end:
		++cont;
	case 4:
		for (;;) {
			uint8_t c;
			if (any_bracket(&c, str) == IE_CONT)
				return IE_CONT;
			pop_frame(z);

			if (c == ')') {
				if (at_root_frame(z))
					break;
				cur = curnode(z);

				*parent_len(z) += cur->header.rec.len;
				if (encode_header(&cur->header, &z->acc, str)
				    != 0)
					return IE_CONT; /* error */

				*parent_len(z) += cur->header.enc.size;
			} else if (c == '(') {
				cur = cur->next = new_zeroed(struct Node);
				push_frame(cur, z);

				goto header;
			} else {
				assert(0 == 1);
			}
		}

		break;
	default:
		assert(0 == 1);
	}

	if (!list_empty(&z->bt)) {
		assert(at_root_frame(z));
		if (encode_header(&curnode(z)->header, &z->acc, str) != 0)
			return IE_CONT;
	}

	cont = 0;
	return IE_DONE;
}

/* Write Pascal string to stdout */
static inline void
putps(const struct Pstring *s)
{
	fwrite(s->data, s->size, 1, stdout);
}

/* Write encoded data to stdout; free allocated resources */
static void
write_tree(struct EncSt *z)
{
	assert(at_root_frame(z));

	struct Node *cur;
	for (; (cur = curnode(z)) != NULL; free(cur)) {
		putps(&cur->header.enc);
		if (cur->contents != NULL) {
			putps(cur->contents);
			free(cur->contents);
		}

		if (cur->next != NULL) {
			list_first_entry(&z->bt, struct Frame, h)->node =
				cur->next;
			if (cur->child != NULL)
				push_frame(cur->child, z);
		} else if (cur->child != NULL) {
			list_first_entry(&z->bt, struct Frame, h)->node =
				cur->child;
		} else {
			pop_frame(z);
		}
	}

	buffer_reset(&z->acc);
}

IterV
encode(struct EncSt *z, struct Stream *str)
{
	if (str->type == S_EOF) {
		if (list_empty(&z->bt)) {
			return IE_DONE;
		} else {
			set_error(str, "Unexpected EOF");
			return IE_CONT;
		}
	}

	for (;;) {
		if (read_tree(z, str) == IE_CONT)
			return IE_CONT;

		if (!list_empty(&z->bt)) {
			write_tree(z);
			assert(list_empty(&z->bt));
		}
	}
}
