
lib_LTLIBRARIES = libuprof.@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@.la

libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_SOURCES = uprof.c

#libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_LDADD =
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_LDFLAGS = -export-dynamic
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_CFLAGS = @EXTRA_CFLAGS@
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_CPPFLAGS = \
    -DGNOME_ICONDIR=\""$(datadir)/pixmaps"\" \
    -DSHARE_DIR=\"$(pkgdatadir)\" \
    -DUPROF_LOCALEDIR=\""$(uproflocaledir)"\" \
    -DUPROF_BIN_DIR=\""$(bindir)"\" \
    -I$(top_srcdir) \
    -I$(top_srcdir)/src \
    @EXTRA_CPPFLAGS@

#EXTRA_DIST = 

#@INTLTOOL_DESKTOP_RULE@

