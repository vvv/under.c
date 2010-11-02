## ---------------------------------------------------------------------
## Configurable section

# CPPFLAGS = -DDEBUG -DFILLERS
CPPFLAGS = -DFILLERS

CFLAGS = -g -Wall -Wextra
# CFLAGS = -O3 -Wall -Wextra

# LDFLAGS = -rdynamic
LDLIBS = -ldl

PROG = under
SRC = iteratee.c decoder.c encoder.c codec.c under.c util.c repr.c buffer.c

## ---------------------------------------------------------------------
## The stuff below is not supposed to be touched frequently

SHELL = /bin/sh

all: $(PROG)

OBJ := $(SRC:.c=.o)
-include $(OBJ:.o=.d)

# Generate dependencies
%.d: %.c
	cpp -MM $(CPPFLAGS) $< |\
 sed -r 's%^(.+)\.o:%$(@D)/\1.d $(@D)/\1.o:%' >$@

$(PROG): $(OBJ)
	$(CC) $(LDFLAGS) $(LDLIBS) $^ -o $@

mostlyclean:
	rm -f $(OBJ) $(OBJ:.o=.d)

clean: mostlyclean
	rm -f $(PROG)

.PHONY: mostlyclean clean
