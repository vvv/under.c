SHELL = /bin/sh
.PHONY: clean check test

CFLAGS = -g -DDEBUG -Wall -Wextra
# CFLAGS = -O3 -Wall -Wextra

PROG = under
SRC = under.c

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
	rm -f $(PROG) _[0-9]_*.log
