## ---------------------------------------------------------------------
## Configurable section

# CPPFLAGS = -DDEBUG
CFLAGS := -g -Wall -Wextra
# CFLAGS := -O3 -Wall -Wextra

SRC = sr.c

## ---------------------------------------------------------------------
## The stuff below is not supposed to be touched frequently

SHELL = /bin/sh
CFLAGS += -fpic
LDFLAGS = -shared

SONAMES := $(SRC:%.c=libunder_%.so)

all: $(SONAMES)

OBJ := $(SRC:.c=.o)
-include $(OBJ:.o=.d)

# Generate dependencies
%.d: %.c
	cpp -MM -MG $(CPPFLAGS) $< |\
 sed -r 's%^(.+)\.o:%$(@D)/\1.d $(@D)/\1.o:%' >$@

libunder_%.so: %.o
	$(CC) $(LDFLAGS) $(LDLIBS) $^ -o $@
# XXX What about versioning info?
# 	gcc -Wall -W -shared -fpic -Wl,-soname,libunder_sr.so.1\
#  -o libunder_sr.so.1.0.0 sr.c

ctt.h: ctt.in gen-ctt.sh
	./gen-ctt.sh $< >$@

mostlyclean:
	rm -f $(OBJ) $(OBJ:.o=.d) ctt.h

clean: mostlyclean
	rm -f $(SONAMES)

.PHONY: mostlyclean clean
