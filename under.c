#include <stdio.h>

int
main(int argc, char *argv[])
{
	int i = 0;
	fputs("XXX", stderr);
	for (i = 0; i < argc; i++)
		fprintf(stderr, " %s", argv[i]);
	fputc('\n', stderr);
	return 0;
}
