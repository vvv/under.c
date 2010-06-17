SHELL = /bin/sh
.PHONY: clean

# CPPFLAGS = -DDEBUG
# CFLAGS = -g -Wall -Wextra
CFLAGS = -O3 -Wall -Wextra

PROG = under
SRC = iteratee.c decoder.c encoder.c codec.c under.c util.c

$(PROG): $(SRC)

clean:
	rm -f $(PROG)
