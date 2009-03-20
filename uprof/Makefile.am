
lib_LTLIBRARIES = libuprof-@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@.la

h_source = uprof.h uprof-private.h
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_SOURCES = \
	uprof.c \
	$(h_source)

#libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_LDADD =
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_LDFLAGS = \
	-export-dynamic \
	@UPROF_DEP_LIBS@
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_CFLAGS = \
	@EXTRA_CFLAGS@ \
	@UPROF_DEP_CFLAGS@
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_CPPFLAGS = \
    	-DSHARE_DIR=\"$(pkgdatadir)\" \
    	-DUPROF_LOCALEDIR=\""$(uproflocaledir)"\" \
    	-DUPROF_BIN_DIR=\""$(bindir)"\" \
    	-I$(top_srcdir) \
    	-I$(top_srcdir)/src \
    	@EXTRA_CPPFLAGS@

uprofincludedir = $(includedir)/uprof-@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@
uprofinclude_HEADERS = $(h_source)

#EXTRA_DIST =

