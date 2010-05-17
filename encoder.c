#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "encoder.h"
#include "asn1.h"
#include "util.h"

static inline bool
_isspace(unsigned char c)
{
	return isspace(c);
}

/* Parse '\s*\(' regexp */
static IterV
left_bracket(struct Stream *str)
{
	if (drop_while(_isspace, str) == IE_CONT)
		return IE_CONT;

	if (*str->data == '(') {
		++str->data;
		--str->size;
		return IE_DONE;
	}

	set_error(&str->errmsg, "`(' expected");
	return IE_CONT;
}

/* Parse '\s*[)(]' regexp, saving met bracket in `*c' */
static IterV
any_bracket(unsigned char *c, struct Stream *str)
{
        if (drop_while(_isspace, str) == IE_CONT)
                return IE_CONT;

        if (*str->data != ')' && *str->data != '(') {
		set_error(&str->errmsg, "`)' or `(' expected");
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
		set_error(&str->errmsg, "`(' or `\"' expected");
		return IE_CONT;
	}

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '\s*[uacp]' regexp */
static IterV
read_tag_class(enum Tag_Class *dest, struct Stream *str)
{
	if (drop_while(_isspace, str) == IE_CONT)
		return IE_CONT;

	switch (*str->data) {
	case 'u': *dest = TC_UNIVERSAL; break;
	case 'a': *dest = TC_APPLICATION; break;
	case 'c': *dest = TC_CONTEXT; break;
	case 'p': *dest = TC_PRIVATE; break;
	default:
		set_error(&str->errmsg, "Invalid tag class specification");
		return IE_CONT;
	}

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '[0-9]+\s' regexp */
static IterV
read_tag_number(unsigned int *dest, struct Stream *str)
{
	static unsigned int n = 0; /* number of parsed digits */

	for (; str->size > 0 && isdigit(*str->data);
	     ++str->data, --str->size) {
		if (++n > 7) {
			set_error(&str->errmsg,
				  "Invalid tag number (too many digits)");
			return IE_CONT;
		}

		*dest = 10 * (*dest) + (*str->data - '0');
	}

	if (str->size == 0)
		return IE_CONT;

	if (n == 0) {
		set_error(&str->errmsg, "Digit expected");
		return IE_CONT;
	}
	n = 0;

	if (!isspace(*str->data)) {
		set_error(&str->errmsg, "White-space character expected");
		return IE_CONT;
	}

	++str->data;
	--str->size;
	return IE_DONE;
}

/* Parse '\s*[uacp][0-9]+\s+' regexp */
IterV
read_header(struct ASN1_Header *tag, struct Stream *str)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		if (read_tag_class(&tag->cls, str) == IE_CONT)
			return IE_CONT;

		tag->num = 0;
		cont = 1;
	case 1:
		if (read_tag_number(&tag->num, str) == IE_CONT)
			return IE_CONT;

		cont = 2;
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

/* Append byte `c' to `dest' */
static IterV
putbyte(unsigned char c, struct Pstring *dest, char **errmsg)
{
	if (dest->size == 0) {
		set_error(errmsg, "Insufficient capacity of encoded bytes'"
			  " accumulator");
		return IE_CONT;
	}

	*dest->data = c;
	++dest->data;
	--dest->size;
	return IE_DONE;
}

/* Parse '\s*([0-9a-zA-Z]{2}(\s+[0-9a-zA-Z]{2})*\s*)?"' regexp */
static IterV
_primval(struct Pstring *dest, struct EncSt *z, struct Stream *str)
{
	static unsigned char nibble = 0;
	static bool expect_space = false;

	unsigned char c;
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
				set_error(&str->errmsg, "White-space character"
					  " expected");
				return IE_CONT;
			}
		}

		if (!isxdigit(c)) {
			set_error(&str->errmsg, "Hexadecimal digit expected");
			return IE_CONT;
		}

		if (nibble == 0) {
			nibble = c;
			continue;
		} else {
			const char s[] = { nibble, c, 0 };
			if (putbyte(strtoul(s, NULL, 16), &z->acc,
				    &str->errmsg) == IE_CONT)
				return IE_CONT;
			++dest->size;

			nibble = 0;
			expect_space = true;
		}
	}
}

/* Parse '\s*([0-9a-zA-Z]{2}(\s+[0-9a-zA-Z]{2})*\s*)?"\s*\)' regexp */
static IterV
read_primitive(struct Pstring *dest, struct EncSt *z, struct Stream *str)
{
	static int cont = 0;

	switch (cont) {
	case 0:
		dest->data = z->acc.data;
		dest->size = 0;

		cont = 1;
	case 1:
		if (_primval(dest, z, str) == IE_CONT)
			return IE_CONT;

		cont = 2;
	case 2:
		if (drop_while(_isspace, str) == IE_CONT)
			return IE_CONT;

		if (*str->data != ')') {
			set_error(&str->errmsg, "`)' expected");
			return IE_CONT;
		}
		++str->data;
		--str->size;

		break;
	default:
		assert(0 == 1);
		return -1;
	}

	debug_print("read_primitive: %lu bytes read",
		    (unsigned long) dest->size);
	cont = 0;
	return IE_DONE;
}

union U_Header {
	struct ASN1_Header rec; /* Intermediate representation */
	struct Pstring enc; /* DER encoding */
};

static void
encode_header(union U_Header *dest)
{
/* XXX ! */
	const struct ASN1_Header *h = &dest->rec;
	debug_print("encode_header: %c%u %s %lu", "uacp"[h->cls], h->num,
		    h->cons_p ? "cons" : "prim", (unsigned long) h->len);
}

/* Node of the ``encoding tree'' */
struct Node {
	struct Node *next, /* Next sibling; NULL for the last node */
		*child; /* First child of this node; NULL for a leaf node */

	union U_Header header; /* Tag header -- i.e., identifier and length */

	/* DER encoding of primitive contents; NULL for constructed tags */
	struct Pstring *contents;
};

/* An element of backtrace */
struct Frame {
	struct list_head h;
	struct Node *node; /* Encoding tree node this frame points to */
};

/* Return the current node or NULL, if the backtrace is empty */
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
parent(const struct EncSt *z)
{
	assert(!list_empty(&z->bt));
	assert(!list_is_last(z->bt.next, &z->bt));
	return list_entry(&z->bt.next->next, struct Frame, h)->node;
}

/* Test whether there is only one frame in the backtrace */
static bool
at_root_frame(const struct EncSt *z)
{
	assert(!list_empty(&z->bt));
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
	static int cont = 0;

	if (str->type == S_EOF) {
		if (list_empty(&z->bt)) {
			return IE_DONE;
		} else {
			set_error(&str->errmsg, "Unexpected EOF");
			return IE_CONT;
		}
	}

	struct Node *cur = curnode(z);

	switch (cont) {
	case 0:
		assert(list_empty(&z->bt));

		if (left_bracket(str) == IE_CONT)
			return IE_CONT;

		cur = new_zeroed(struct Node);
		push_frame(cur, z);

header:
		cont = 1;
	case 1:
		if (read_header(&cur->header.rec, str) == IE_CONT)
			return IE_CONT;

		cont = 2;
	case 2:
		if (contents_type(&cur->header.rec.cons_p, str) == IE_CONT)
			return IE_CONT;

		if (cur->header.rec.cons_p) {
			cur = cur->child = new_zeroed(struct Node);
			push_frame(cur, z);

			goto header;
		} else {
			cur->contents = new_zeroed(struct Pstring);
		}

		cont = 3;
	case 3:
		if (read_primitive(cur->contents, z, str) == IE_CONT)
			return IE_CONT;

		cur->header.rec.len = cur->contents->size;
		encode_header(&cur->header);

		if (at_root_frame(z))
			break; /* done */

		parent(z)->header.rec.len +=
			cur->header.enc.size + cur->contents->size;

		cont = 4;
	case 4:
		for (;;) {
			unsigned char c;
			if (any_bracket(&c, str) == IE_CONT)
				return IE_CONT;
			pop_frame(z);

			if (c == ')') {
				if (at_root_frame(z))
					break; /* done */

				cur = curnode(z);
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

	cont = 0;
	return IE_DONE;
}

static void
write_tree(struct EncSt *z)
{
/* XXX ! */
	assert(at_root_frame(z));
	debug_print("XXX write_tree");
}

IterV
encode(struct EncSt *z, struct Stream *str)
{
	for (;;) {
		if (read_tree(z, str) == IE_CONT)
			return IE_CONT;

		write_tree(z);
		assert(list_empty(&z->bt));
	}
}
