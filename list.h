#ifndef _LIST_H
#define _LIST_H

#include <stddef.h>

/*
 * Most of the code is copied verbatim from <linux/list.h>
 * [http://lxr.linux.no/#linux+v2.6.33/include/linux/list.h].
 *
 * `container_of' comes from <linux/kernel.h>.
 */

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr: the pointer to the member.
 * @type: the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type,member)); })
#endif

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_last(const struct list_head *list,
			       const struct list_head *head)
{
	return list->next == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/**
 * list_entry - get [the pointer to] the struct for this entry
 * @ptr: the &struct list_head pointer.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr: the list head to take the element from.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

/**
 * __list_for_each - iterate over a list
 * @pos: the &struct list_head to use as a loop cursor.
 * @head: the head for your list.
 *
 * This variant differs from list_for_each() in that it's the
 * simplest possible list iteration code, no prefetching is done.
 * Use this for code that knows the list to be very short (empty
 * or 1 entry) most of the time.
 */
#define __list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *n)
{
	n->next = NULL;
	n->pprev = NULL;
}

/* static inline int hlist_empty(const struct hlist_head *h) */
/* { */
/* 	return !h->first; */
/* } */

/* static inline void __hlist_del(struct hlist_node *n) */
/* { */
/* 	struct hlist_node *next = n->next; */
/* 	struct hlist_node **pprev = n->pprev; */
/* 	*pprev = next; */
/* 	if (next) */
/* 		next->pprev = pprev; */
/* } */

/* Make node `n' the head of the hlist pointed to by `h' */
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

/* /\* */
/*  * Insert node `n' before `next'. */
/*  * */
/*  * next must be != NULL */
/*  *\/ */
/* static inline void hlist_add_before(struct hlist_node *n, */
/* 				    struct hlist_node *next) */
/* { */
/* 	n->pprev = next->pprev; */
/* 	n->next = next; */
/* 	next->pprev = &n->next; */
/* 	*(n->pprev) = n; */
/* } */

/* /\* Insert node `next' after `n'. *\/ */
/* static inline void hlist_add_after(struct hlist_node *n, */
/* 				   struct hlist_node *next) */
/* { */
/* 	next->next = n->next; */
/* 	n->next = next; */
/* 	next->pprev = &n->next; */

/* 	if (next->next) */
/* 		next->next->pprev  = &next->next; */
/* } */

/**
 * hlist_entry - get [the pointer to] the struct for this entry
 * @ptr: the &struct hlist_node pointer.
 * @type: the type of the struct this is embedded in.
 * @member: the name of the hlist_node within the struct.
 */
#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

/* #define hlist_for_each(pos, head) \ */
/* 	for (pos = (head)->first; pos; pos = pos->next) */

#define hlist_for_each_safe(pos, n, head) \
	for (pos = (head)->first; pos && ({ n = pos->next; 1; }); pos = n)

/**
 * hlist_for_each_entry	- iterate over list of given type
 * @tpos:	the type * to use as a loop cursor.
 * @pos:	the &struct hlist_node to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry(tpos, pos, head, member)                         \
	for (pos = (head)->first;                                             \
	     pos && ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1; }); \
	     pos = pos->next)

#endif /* _LIST_H */
