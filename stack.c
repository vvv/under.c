#include "stack.h"
#include "util.h"

void
stack_push(struct Stack **head, unsigned int value)
{
	struct Stack *new = xmalloc(sizeof(struct Stack));

	new->value = value;
	new->next = *head;
	*head = new;
}

unsigned int
stack_pop(struct Stack **head)
{
	unsigned int rv = (*head)->value;

	void *x = *head;
	*head = (*head)->next;
	free(x);

	return rv;
}

/* void */
/* stack_free(struct Stack **head) */
/* { */
/* 	while (*head != NULL) { */
/* 		void *x = head; */
/* 		*head = (*head)->next; */
/* 		free(x); */
/* 	} */
/* } */
