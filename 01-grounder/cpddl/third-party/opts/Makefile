###
# opts
# -----
# Copyright (c)2010-2012 Daniel Fiser <danfis@danfis.cz>
#
#  This file is part of opts.
#
#  Distributed under the OSI-approved BSD License (the "License");
#  see accompanying file BDS-LICENSE for details or see
#  <http://www.opensource.org/licenses/bsd-license.php>.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even the
#  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the License for more information.
##

-include Makefile.local
-include Makefile.include

CFLAGS += -I.
LDFLAGS += -L. -lopts -lm

TARGETS  = libopts.a
TARGETS += test test2
OBJS  = opts.o


all: $(TARGETS)

libopts.a: $(OBJS)
	ar cr $@ $(OBJS)
	ranlib $@

test: test.c libopts.a
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@if ! $(SH) unittest.sh; then echo "Unit test FAILED."; exit -1; fi
test2: test2.c libopts.a
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
#./test2 -o 2.2 -h -a 2,3.1,11

opts.c: parse.c optarg.c
	touch opts.c

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: libopts.a doc
	mkdir -p $(PREFIX)/$(INCLUDEDIR)
	mkdir -p $(PREFIX)/$(LIBDIR)
	mkdir -p $(PREFIX)/$(MANDIR)
	cp opts.h $(PREFIX)/$(INCLUDEDIR)
	cp libopts.a $(PREFIX)/$(LIBDIR)
	cp opts.3 $(PREFIX)/$(MANDIR)

clean:
	rm -f $(OBJS)
	rm -f $(TARGETS)
	rm -f *.3 *.3.gz
	if [ -d testsuites ]; then $(MAKE) -C testsuites clean; fi;
	if [ -d doc ]; then $(MAKE) -C doc clean; fi;

doc: opts.3

opts.3: opts.3.txt
	a2x -d manpage -f manpage $<
	if [ -f opts,_optsAdd,_optsAddDesc,_optsClear,_optsPrint.3 ] \
        && [ ! -f opts.3 ]; then \
            mv opts,_optsAdd,_optsAddDesc,_optsClear,_optsPrint.3 opts.3; \
    fi;

html-doc:
	asciidoc -d manpage -b html5 -s -f man-html.conf opts.3.txt

analyze: clean
	$(SCAN_BUILD) $(MAKE)


deb:
	$(PYTHON) debian/make-changelog.py <CHANGELOG >debian/changelog
	cp BSD-LICENSE debian/copyright
	debuild -us -uc

help:
	@echo "Targets:"
	@echo "    all        - Build library"
	@echo "    doc        - Build documentation"
	@echo "    clean      - Remove all generated files"
	@echo "    install    - Install library into system"
	@echo "    analyze    - Performs static analysis using Clang Static Analyzer"
	@echo ""
	@echo "Options:"
	@echo "    CC         - Path to C compiler           (=$(CC))"
	@echo "    SH         - Path to sh(1)                (=$(SH))"
	@echo "    SCAN_BUILD - Path to scan-build           (=$(SCAN_BUILD))"
	@echo "    A2X        - Path to a2x asciidoc program (=$(A2X))"
	@echo "    GZIP       - Path to gzip(1)				 (=$(GZIP))"
	@echo ""
	@echo "    CC_NOT_GCC 'yes'/'no' - If set to 'yes' no gcc specific options will be used (=$(CC_NOT_GCC))"
	@echo ""
	@echo "    DEBUG      'yes'/'no' - Turn on/off debugging          (=$(DEBUG))"
	@echo "    PROFIL     'yes'/'no' - Compiles profiling info        (=$(PROFIL))"
	@echo "    NOWALL     'yes'/'no' - Turns off -Wall gcc option     (=$(NOWALL))"
	@echo "    NOPEDANTIC 'yes'/'no' - Turns off -pedantic gcc option (=$(NOPEDANTIC))"
	@echo ""
	@echo "    PREFIX     - Prefix where library will be installed                             (=$(PREFIX))"
	@echo "    INCLUDEDIR - Directory where header files will be installed (PREFIX/INCLUDEDIR) (=$(INCLUDEDIR))"
	@echo "    LIBDIR     - Directory where library will be installed (PREFIX/LIBDIR)          (=$(LIBDIR))"
	@echo "    MANDIR     - Directory where manpages will be installed (PREFIX/MANDIR)         (=$(MANDIR))"
	@echo ""
	@echo "Variables:"
	@echo "  Note that most of can be preset or changed by user"
	@echo "    SYSTEM            = $(SYSTEM)"
	@echo "    CFLAGS            = $(CFLAGS)"
	@echo "    LDFLAGS           = $(LDFLAGS)"

.PHONY: all clean check check-valgrind help doc install analyze examples
