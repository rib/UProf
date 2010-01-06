
noinst_PROGRAMS = simple suspend suspend2 dlopen recursion linking sanity_check

AM_CFLAGS = \
	    @EXTRA_CFLAGS@ \
	    @UPROF_DEP_CFLAGS@ \
	    -I$(top_srcdir)/ \
	    -I$(top_srcdir)/uprof \
	    -I$(top_builddir)/uprof
AM_LDFLAGS = \
	     @UPROF_DEP_LIBS@ \
	     $(top_builddir)/uprof/libuprof-@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@.la

simple_SOURCES = simple.c
suspend_SOURCES = suspend.c
linking_SOURCES = linking.c
dlopen_SOURCES = dlopen.c
dlopen_LDFLAGS = \
		 -export-dynamic \
		 $(AM_LDFLAGS)

dlopen_LDADD = -ldl

all-local: module.so
include ./$(DEPDIR)/module.Po
module.so: module.c
	$(CC) -c module.c -MT module.o -MD -MP -MF .deps/module.Tpo -fPIC $(AM_CFLAGS)
	$(CC) -o module.so -shared module.o
clean-local:
	rm -f module.so
