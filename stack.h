#ifndef _STACK_H
#define _STACK_H

/* Singly-linked list. */
struct Stack {
	unsigned int value;
	struct Stack *next;
};

/*
 * Add a `value' to the stack.
 *
 * `head' must be a valid pointer.
 * `*head' may be NULL.
 */
void stack_push(struct Stack **head, unsigned int value);

/*
 * Remove an element from the stack and return its `value'.
 *
 * `*head' must be a valid pointer.
 */
unsigned int stack_pop(struct Stack **head);

/* void stack_free(struct Stack **head); */

#endif /* _STACK_H */
