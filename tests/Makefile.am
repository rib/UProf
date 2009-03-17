
noinst_PROGRAMS = simple

AM_CFLAGS = \
	    @EXTRA_CFLAGS@ \
	    @UPROF_DEP_CFLAGS@ \
	    -I$(top_srcdir)/ \
	    -I$(top_srcdir)/uprof \
	    -I$(top_builddir)/uprof
AM_LDFLAGS = \
	     @UPROF_DEP_LIBS@ \
	     $(top_builddir)/uprof/libuprof.@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@.la

simple_SOURCES = simple.c

