.POSIX:
.PHONY: clean install check test basic_test_shared basic_test_static test-config\
		unnecessary uninstall installib installshared \
		installstatic installsidefile installheader installcfg

NAME = libunixcompat
LIBNAME = ./unixcompat/libunixcompat
LINKNAME = unixcompat

CC = /bin/xlc
LD = /bin/xlc
AR = ar
ARFLAGS = rc
OPT = -Os
QFLAGS = -qdll -qexportall -qlongname -qascii -q64 -qnocse -qfloat=ieee \
		 -qgonum -qasm -qbitfield=signed -qlanglvl=extc1x -qsuppress=CCN3684
DFLAGS = -D__MV17195__ -D_XOPEN_SOURCE=600
COMMONFLAGS = $(OPT) $(QFLAGS)
LDFLAGS = $(COMMONFLAGS)
SOFLAGS = $(COMMONFLAGS) -Wl,dll
CFLAGS = $(COMMONFLAGS) $(DFLAGS)


# These flags are ESSENTIAL to get anything to work with unixcompat
# They should be added to the .cfg file
# NOTE: things seem to work without nolibansi, even on -O5,
# but leaving it out would make me nervous.
#ESSENTIALFLAGS = -qnolibansi -qnosearch -I./unixcompat -I/usr/include -L.

TESTFLAGS = -F ./unixcompat/xlc_unixcompat.cfg $(CFLAGS)

# make install PREFIX=/my/prefix/path
PREFIX = $(CONDA_PREFIX)
# potential problem for packages that should not be linked against things in
# $PREFIX/lib
LIB_INSTALL_LOC = $(PREFIX)/unixcompat
INCLUDE_INSTALL_LOC = $(PREFIX)/unixcompat
CFG_INSTALL_LOC = $(PREFIX)/unixcompat

# might want to change this at some point
all: shared static
shared: $(LIBNAME).so $(LIBNAME).x
static: $(LIBNAME).a

# .x made during linking of .so
$(LIBNAME).x: $(LIBNAME).so
	mv $(NAME).x ./unixcompat/
	chtag -t -c 1047 $@

$(LIBNAME).so: compatmalloc.o
	$(LD) $(SOFLAGS) -o $@ $^
	extattr +p $@
	chtag -b $@

$(LIBNAME).a: compatmalloc.o
	$(AR) $(ARFLAGS) $@ $^
	chtag -b $@

compatmalloc.o: compatmalloc.c
	$(CC) $(CFLAGS) -c -o $@ $^

# The test script should not be compiled as part of all.
# We are testing the compilation itself, too.
basic_test_shared: basic_test.c unixcompat/stdlib.h shared test-config
	$(CC) $(TESTFLAGS) -o $@ basic_test.c $(LIBNAME).x
	LIBPATH=./unixcompat ./$@

basic_test_static: basic_test.c unixcompat/stdlib.h static test-config
	_C89_LIBDIRS="./unixcompat /lib /usr/lib" $(CC) $(TESTFLAGS) -o $@ basic_test.c
	./$@

unnecessary: unnecessary.c unixcompat/stdlib.h static test-config
	_C89_LIBDIRS="./unixcompat /lib /usr/lib" $(CC) $(TESTFLAGS) -o $@ unnecessary.c
	./$@

# shared doesn't work with the current setup
test: basic_test_static unnecessary
	-@rm ./unixcompat/xlc_unixcompat.cfg

# The following target is only for testing
test-config: xlc_unixcompat.cfg.in
	touch ./unixcompat/xlc_unixcompat.cfg
	chtag -t -c 819 ./unixcompat/xlc_unixcompat.cfg
	sed -e "s:@PREFIX@:$(PWD):g" $^ > ./unixcompat/xlc_unixcompat.cfg

install: all installlib installheader installcfg installxlc

installlib: installshared installstatic

installshared: shared installsidefile
	mkdir -p $(LIB_INSTALL_LOC)
	-rm -f $(LIB_INSTALL_LOC)/$(NAME).so
	cp $(LIBNAME).so $(LIB_INSTALL_LOC)/$(NAME).so
	chmod 555 $(LIB_INSTALL_LOC)/$(NAME).so

installsidefile: $(LIBNAME).x
	mkdir -p $(LIB_INSTALL_LOC)
	-rm -f $(LIB_INSTALL_LOC)/$(NAME).x
	cp $(LIBNAME).x $(LIB_INSTALL_LOC)/$(NAME).x
	chmod 444 $(LIB_INSTALL_LOC)/$(NAME).x

installstatic: static
	mkdir -p $(LIB_INSTALL_LOC)
	-rm -f $(LIB_INSTALL_LOC)/$(NAME).a
	cp $(LIBNAME).a $(LIB_INSTALL_LOC)/$(NAME).a
	chmod 444 $(LIB_INSTALL_LOC)/$(NAME).a

installheader:
	mkdir -p $(INCLUDE_INSTALL_LOC)
	-rm -f $(INCLUDE_INSTALL_LOC)/stdlib.h
	cp -pm ./unixcompat/stdlib.h $(INCLUDE_INSTALL_LOC)/stdlib.h
	chmod 444 $(INCLUDE_INSTALL_LOC)/stdlib.h
	chtag -t -c 819 $(INCLUDE_INSTALL_LOC)/stdlib.h

installcfg:
	mkdir -p $(CFG_INSTALL_LOC)
	-rm -f $(CFG_INSTALL_LOC)/xlc_unixcompat.cfg
	touch $(CFG_INSTALL_LOC)/xlc_unixcompat.cfg
	chtag -t -c 819 $(CFG_INSTALL_LOC)/xlc_unixcompat.cfg
	sed -e "s:@PREFIX@:$(PREFIX):g" xlc_unixcompat.cfg.in > "$(CFG_INSTALL_LOC)/xlc_unixcompat.cfg"
	chmod 444 $(CFG_INSTALL_LOC)/xlc_unixcompat.cfg

uninstall:
	-rm -f $(LIB_INSTALL_LOC)/$(NAME).so
	-rm -f $(LIB_INSTALL_LOC)/$(NAME).a
	-rm -f $(LIB_INSTALL_LOC)/$(NAME).x
	-rm -f $(INCLUDE_INSTALL_LOC)/stdlib.h
	-rm -f $(CFG_INSTALL_LOC)/xlc_unixcompat.cfg

clean:
	-rm -f *.o
	-rm -f *.so
	-rm -f *.a
	-rm -f *.x
	-rm -f *.dbg
	-rm -f *.mdbg
	-rm -f xlc_unixcompat.cfg
	-rm -f basic_test_shared
	-rm -f basic_test_static
	-rm -f xlc
	-rm -f unnecessary
	cd ./unixcompat
	-rm -f xlc_unixcompat.cfg
	-rm -f *.so
	-rm -f *.x
	-rm -f *.a
	cd ..

check: test

xlc: xlc.c
	/bin/xlc -qlanglvl=extc99 -qgonum -o xlc $^

installxlc: xlc xlc.cfg
	mkdir -p "$(PREFIX)/bin"
	cp -pm xlc $(PREFIX)/bin/xlc_test
	cp -pm xlc $(PREFIX)/bin/xlc_test_debug
	cp -pm xlc $(PREFIX)/bin/xlc_echocmd
	cp -pm xlc $(PREFIX)/bin/xlc_echocmd_debug
	cp -pm xlc $(PREFIX)/bin/xlc++
	cp -pm xlc $(PREFIX)/bin/xlc++_test
	cp -pm xlc $(PREFIX)/bin/xlc++_test_debug
	cp -pm xlc $(PREFIX)/bin/xlc++_echocmd
	cp -pm xlc $(PREFIX)/bin/xlc++_echocmd_debug
	iconv -F -T -t 819 xlc.cfg > $(PREFIX)/bin/xlc.cfg
