SHELL = /bin/sh
.PHONY: clean check test

#CPPFLAGS = -DDEBUG
CFLAGS = -g -Wall -Wextra
# CFLAGS = -O3 -Wall -Wextra

PROG = under
SRC = iteratee.c decoder.c encoder.c codec.c under.c util.c

$(PROG): $(SRC)

check test: $(PROG)
	set -e; \
for f in tests/*; do \
    CMD="`head -n1 $$f | sed 's/^\$$ //'`"; \
    OUT=_$${f#tests/}.log; \
    $$CMD >$$OUT || exit 1; \
    tail -n+2 $$f | diff -u $$OUT - && rm $$OUT || exit 1; \
done

clean:
	rm -f $(PROG)
