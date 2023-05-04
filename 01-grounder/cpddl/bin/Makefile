-include ../Makefile.local
-include ../Makefile.include

CFLAGS += -I../
CFLAGS += $(BORUVKA_CFLAGS)
CFLAGS += $(OPTS_CFLAGS)
LDFLAGS += -L../ -lpddl
LDFLAGS += $(BORUVKA_LDFLAGS)
LDFLAGS += $(OPTS_LDFLAGS)
LDFLAGS += $(LP_LDFLAGS)
LDFLAGS += $(BLISS_LDFLAGS)
LDFLAGS += -pthread -lrt -lm

ifeq '$(BIN_STATIC)' 'yes'
  LDFLAGS += -static
endif

TARGETS = pddl-check
TARGETS += pddl-strips
TARGETS += pddl-strips-as-py
TARGETS += pddl-pddl
TARGETS += pddl-lifted-mgroups
TARGETS += pddl-fdr
TARGETS += pddl-lifted-mgroups-exp

all: $(TARGETS)

pddl-%: %.c ../libpddl.a
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS)
	rm -f *.o

.PHONY: all
