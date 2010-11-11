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
#include "buffer.h"
#include "asn1.h"
#include "util.h"
#include "repr.h"

void
free_DecSt(struct DecSt *z)
{
	if (z == NULL)
		return;

	if (z->buf_repr != NULL) {
		free(buffer_data(z->buf_repr));
		free(z->buf_repr);
	}

	if (z->buf_raw != NULL) {
		free(buffer_data(z->buf_raw));
		free(z->buf_raw);
	}

	free(z);
}

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
		debug_print("decode_header, cont=%d", cont);
#ifdef FILLERS
		if (at_root_p && drop_while(isfiller, str) == IE_CONT)
			return IE_CONT;
#endif

		++cont;
	case 1: /* Identifier octet(s) -- cases 1, 2 */
		debug_print("decode_header, cont=%d", cont);

		if (str->type == S_EOF)
			return IE_DONE;

		if (head(&c, str) == IE_CONT)
			return IE_CONT;

		tag->cls = (c & 0xc0) >> 6;
		tag->cons_p = (c & 0x20) != 0;
		debug_print(" \\_ tag_cls = '%c', %s",
			    "uacp"[tag->cls], tag->cons_p ? "cons" : "prim");

		if ((tag->num = c & 0x1f) == 0x1f)
			tag->num = 0; /* tag number > 30 */
		else
			goto tagnum_done;

		++cont;
	case 2: /* Tag number > 30 (``high'' tag number) */
		debug_print("decode_header, cont=%d", cont);

		for (; str->size > 0 && *str->data & 0x80;
		     ++str->data, --str->size)
			tag->num = (tag->num << 7) | (*str->data & 0x7f);

		if (head(&c, str) == IE_CONT)
			return IE_CONT;
		tag->num = (tag->num << 7) | (c & 0x7f);

tagnum_done:
		if (tag->num & 0xc0000000) { /* exceeds 30 bits */
			set_error(str, "Tag number is too big: %u", tag->num);
			return IE_CONT;
		}
		debug_print(" \\_ tag_num = %u", tag->num);

		++cont;
	case 3: /* Initial length octet */
		debug_print("decode_header, cont=%d", cont);

		if (head(&c, str) == IE_CONT)
			return IE_CONT;

		if (c == 0xff) {
			set_error(str, "Length encoding is invalid\n"
				  "  [ITU-T X.690, 8.1.3.5-c]");
			return IE_CONT;
		}

		if (c & 0x80) { /* long form */
			len_sz = c & 0x7f;
			if (len_sz > 8) {
				set_error(str, "Too many octets as for length"
					  " encoding: %lu",
					  (unsigned long) len_sz);
				return IE_CONT;
			}

			debug_print(" \\_ len_sz = %lu",
				    (unsigned long) len_sz);
			tag->len = 0;
		} else { /* short form*/
			tag->len = c;
			debug_print(" \\_ tag_len = %lu",
				    (unsigned long) tag->len);
			break;
		}

		++cont;
	case 4: /* Subsequent length octet(s) */
		debug_print("decode_header, cont=%d", cont);

		for (; str->size > 0 && len_sz > 0;
		     --len_sz, ++str->data, --str->size)
			tag->len = (tag->len << 8) | *str->data;

		if (len_sz > 0)
			return IE_CONT;
		debug_print(" \\_ tag_len = %lu", (unsigned long) tag->len);

		break;
	default:
		assert(0 == 1);
	}

	cont = 0;
	return IE_DONE;
}

/*
 * Print hexadecimal dump of stream contents.
 *
 * @final: Does `*str' contain all the bytes that need to be hexdumped?
 */
static IterV
print_hexdump(struct Stream *str, bool final)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		debug_print("print_hexdump, cont=%d", cont);
		putchar('"');

		++cont;
	case 1:
		debug_print("print_hexdump, cont=%d", cont);
		{
			uint8_t c;
			if (head(&c, str) == IE_CONT) {
				if (final)
					break; /* "empty" tag  (clen == 0) */
				return IE_CONT;
			}
			printf("%02x", c);
		}

		++cont;
	case 2:
		debug_print("print_hexdump, cont=%d", cont);

		for (; str->size > 0; ++str->data, --str->size)
			printf(" %02x", *str->data);

		if (final)
			break;
		return IE_CONT;

	default:
		assert(0 == 1);
	}

	putchar('"');
	cont = 0;
	return IE_DONE;
}

static void
print_hexdump_strict(const uint8_t *src, size_t n)
{
	putchar('"');

	if (n != 0) {
		printf("%02x", *src);
		while (--n != 0)
			printf(" %02x", *(++src));
	}

	putchar('"');
}

static struct Buffer *
new_buffer(size_t size)
{
	struct Buffer *buf = xmalloc(sizeof(struct Buffer));
	INIT_BUFFER(buf);

	if (buffer_resize(buf, size) != 0)
		die("Out of memory, buffer_resize failed");

	return buf;
}

static int
store(struct Buffer *dest, const void *src, size_t n, struct Stream *str)
{
	int r;
	if ((r = buffer_put(dest, src, n)) != 0)
		set_error(str, "Insufficient capacity of raw bytes'"
			  " accumulator");
	return r;
}

#ifdef DEBUG
#  define call(fp, dest, src, n) (fp)((dest), (src), (n))
#else
/* Call `fp', appending trailing '\0' to the buffer. */
static inline int
call(Repr_Codec fp, struct Buffer *dest, const uint8_t *src, size_t n)
{
	const int r = fp(dest, src, n);
	*dest->wptr = 0; /* there's a reserved byte in a buffer */
	return r;
}
#endif

/*
 * Print representation of a primitive encoding.
 *
 * @str: Pointer to the stream that contains primitive encoding
 *       (probably, only part of it).
 * @enough: Are there enough bytes in the stream to reach the end of encoding?
 * @_decode: Pointer to the function that converts raw bytes to
 *           human-friendly representation.
 */
static IterV
print_prim(struct Stream *str, bool enough, Repr_Codec _decode, struct DecSt *z)
{
	assert(str->type == S_CHUNK);

	if (_decode == NULL)
		return print_hexdump(str, enough);
	else if (z->buf_repr == NULL)
		z->buf_repr = new_buffer(128);

	static int cont = 0;
	debug_print("print_prim: cont=%d", cont);

	switch (cont) {
	case 0:
		if (enough) {
			if (call(_decode, z->buf_repr, str->data, str->size)
			    == 0) {
				printf("[%s]", buffer_data(z->buf_repr));
			} else {
				fprintf(stderr, "*WARNING* print_prim: %s\n",
					buffer_data(z->buf_repr));
				print_hexdump_strict(str->data, str->size);
			}

			break;
		}

		if (z->buf_raw == NULL)
			z->buf_raw = new_buffer(64);

		++cont;
	case 1:
		if (store(z->buf_raw, str->data, str->size, str) != 0)
			return IE_CONT;

		if (!enough) {
			str->data += str->size;
			str->size = 0;
			return IE_CONT;
		}

		{
			const uint8_t * const raw = buffer_data(z->buf_raw);
			const size_t n = buffer_len(z->buf_raw);

			if (call(_decode, z->buf_repr, raw, n) == 0) {
				printf("[%s]", buffer_data(z->buf_repr));
			} else {
				fprintf(stderr, "*WARNING* print_prim: %s\n",
					buffer_data(z->buf_repr));

				print_hexdump_strict(raw, n);
			}
		}

		break;
	default:
		assert(0 == 1);
	}

	str->data += str->size;
	str->size = 0;

	buffer_reset(z->buf_repr);
	if (z->buf_raw != NULL)
		buffer_reset(z->buf_raw);

	cont = 0;
	return IE_DONE;
}

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
	debug_print("add_capacity %lu", (unsigned long) n);

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
close_drained_containers(struct DecSt *z)
{
	struct list_head *p, *t;

	list_for_each_safe(p, t, &z->caps) {
		if (capacity(p) != 0)
			break;

		__list_del(p->prev, p->next);
		free(list_entry(p, struct Capacity, h));
		debug_print("zero capacity deleted");

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
			set_error(master, "Unexpected EOF");
			return IE_CONT;
		}
	}

	struct Stream str; /* Substream being passed to an iteratee */
	str.type = master->type;
	str.data = master->data;
	str.errmsg = master->errmsg;

	for (;;) {
		const size_t orig_size = z->depth == 0 ?
			master->size : MIN(remcap(z), master->size);
		str.size = orig_size;
		debug_show_decoder_state(z, &str, master, " %s", header_p ?
					 "decode_header" : "print_prim");

		const IterV indic = header_p
#ifdef FILLERS
			? decode_header(&tag, &str, z->depth == 0)
#else
			? decode_header(&tag, &str)
#endif
			: print_prim(&str, remcap(z) <= str.size,
				     repr_from_raw(z->repr, tag.cls, tag.num),
				     z);
		assert(indic == IE_DONE || indic == IE_CONT);

		decrease_capacities(orig_size - str.size, z);

		master->data = str.data;
		master->size -= orig_size - str.size;
		master->errmsg = str.errmsg;
		debug_show_decoder_state(z, &str, master, " => IE_%s",
					 indic == IE_DONE ? "DONE" : "CONT");

		if (indic == IE_CONT) {
			if (z->depth > 0 && remcap(z) == 0)
				set_error(master, "Trying to go beyond the"
					  " end of container");

			return IE_CONT;
		}

		/* IE_DONE */
		if (header_p) {
			putchar('(');
			repr_show_header(z->repr, tag.cls, tag.num);

			if (tag.len == 0) {
				fputs(tag.cons_p ? " ()" : " \"\"", stdout);
				add_capacity(0, z);
			}

			close_drained_containers(z);

			if (tag.len == 0)
				goto line_feed;

			if (!contained_p(tag.len, z)) {
				set_error(master, "Tag is too big for its"
					  " container");
				return IE_CONT;
			}
			add_capacity(tag.len, z);

			if (!tag.cons_p) {
				header_p = false;
				putchar(' ');
				continue;
			}
		} else {
			close_drained_containers(z);
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
