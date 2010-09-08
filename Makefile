## ---------------------------------------------------------------------
## Configurable section

# CPPFLAGS = -DDEBUG -DFILLERS
CPPFLAGS = -DFILLERS
CFLAGS = -g -Wall -Wextra
# CFLAGS = -O3 -Wall -Wextra

PROG = under
SRC = iteratee.c decoder.c encoder.c codec.c under.c util.c
PLUGINS = sr
## ---------------------------------------------------------------------

SHELL = /bin/sh

all: $(PROG) $(PLUGINS:%=libunder_%.so)

$(PROG): $(SRC)

libunder_%.so: plugins/%.c
	$(CC) -shared -fpic $(CFLAGS) $(CPPFLAGS) $^ -o $@
# XXX What about versioning info?
# 	gcc -Wall -W -shared -fpic -Wl,-soname,libunder_sr.so.1\
#  -o libunder_sr.so.1.0.0 plugins/sr.c

clean:
	rm -f $(PROG) $(PLUGINS:%=libunder_%.so)

.PHONY: clean
