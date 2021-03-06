## ---------------------------------------------------------------------
## Configurable section

# CPPFLAGS = -DDEBUG
CFLAGS := -g -Wall -Wextra
# CFLAGS := -O3 -Wall -Wextra

SRC = common.c sr.c

## ---------------------------------------------------------------------
## The stuff below is not supposed to be touched frequently

SHELL = /bin/sh
CFLAGS += -fpic
LDFLAGS = -shared

SONAMES := $(SRC:%.c=libunder_%.so.0)
SYMLINKS := $(basename $(SONAMES))

all: $(SONAMES) $(SYMLINKS)

OBJ := $(SRC:.c=.o)
-include $(OBJ:.o=.d)

# Generate dependencies
%.d: %.c
	cpp -MM -MG $(CPPFLAGS) $< |\
 sed -r 's%^(.+)\.o:%$(@D)/\1.d $(@D)/\1.o:%' >$@

%.so: %.so.0
	ln -s $^ $@

libunder_%.so.0: %.o
	$(CC) $(LDFLAGS) -Wl,-soname,$@ $^ -o $@
# XXX Output file should be versioned.
# http://tldp.org/HOWTO/Program-Library-HOWTO/shared-libraries.html#AEN95

# decode_TBCDstring(), used by libunder_sr.so, is defined in libunder_common.so
libunder_sr.so.0: sr.o libunder_common.so
	$(CC) $(LDFLAGS) -Wl,-soname,$@ -L. -lunder_common sr.o -o $@

ctt.h: ctt.in gen-ctt.sh
	./gen-ctt.sh $< >$@

mostlyclean:
	rm -f $(OBJ) $(OBJ:.o=.d) ctt.h

clean: mostlyclean
	rm -f $(SONAMES) $(SYMLINKS)

.PHONY: mostlyclean clean
