SHELL = /bin/sh
.PHONY: clean

# CPPFLAGS = -DDEBUG -DFILLERS
# CFLAGS = -g -Wall -Wextra

CPPFLAGS = -DFILLERS
CFLAGS = -O3 -Wall -Wextra

PROG = under
SRC = iteratee.c decoder.c encoder.c codec.c under.c util.c

$(PROG): $(SRC)

clean:
	rm -f $(PROG)
