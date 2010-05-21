#include <stdarg.h>
#include <stdio.h>
#include "util.h"
#include "iteratee.h"

void
set_error(char **errmsg, const char *format, ...)
{
	if (*errmsg != NULL)
		return; /* keep the original error message */

	va_list ap;
	va_start(ap, format);

	*errmsg = xmalloc(80);
	const size_t nchars = vsnprintf(*errmsg, 80, format, ap);
	if (nchars >= 80) {
		/* Not enough space.  Reallocate buffer .. */
		*errmsg = xrealloc(*errmsg, nchars + 1);

		 /* .. and try again. */
		vsnprintf(*errmsg, nchars + 1, format, ap);
	}

	va_end(ap);
}

IterV
head(uint8_t *c, struct Stream *str)
{
	if (str->type == S_EOF) {
		set_error(&str->errmsg, "head: EOF");
		return IE_CONT;
	}

	if (str->size == 0)
		return IE_CONT;

	*c = *str->data;
	++str->data;
	--str->size;

	return IE_DONE;
}

IterV
drop_while(bool (*p)(uint8_t c), struct Stream *str)
{
	for (; str->size > 0; ++str->data, --str->size) {
		if (!p(*str->data))
			return IE_DONE;
	}
	return IE_CONT;
}
