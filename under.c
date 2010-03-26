#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <string.h>

static inline int
streq(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}

static int
decode(const char *path)
{
	FILE *f = NULL;
	if (streq(path, "-")) {
		f = stdin;
	} else if ((f = fopen(path, "rb")) == NULL) {
		error(0, errno, path);
		return 1;
	}

	// XXX Here goes decoding stuff...

	return streq(path, "-") ? 0 : fclose(f);
}

int
main(int argc, char **argv)
{
	if (argc == 1)
		return decode("-");

	int rv = 0, i;
	for (i = 1; i < argc; i++) {
		rv |= decode(argv[i]);
	}
	return rv;
}
