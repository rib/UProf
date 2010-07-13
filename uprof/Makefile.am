
lib_LTLIBRARIES = libuprof-@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@.la

public_h_source = \
	uprof.h \
	uprof-context.h \
	uprof-object-state.h \
	uprof-counter.h \
	uprof-counter-result.h \
	uprof-timer.h \
	uprof-timer-result.h \
	uprof-report.h \
	uprof-dbus.h

libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_SOURCES = \
	uprof-private.h \
	uprof.c \
	uprof-context-private.h \
	uprof-context.c \
	uprof-object-state-private.h \
	uprof-object-state.c \
	uprof-counter.c \
	uprof-counter-result-private.h \
	uprof-counter-result.c \
	uprof-timer.c \
	uprof-timer-result-private.h \
	uprof-timer-result.c \
	uprof-service-private.h \
	uprof-service.c \
	uprof-report-private.h \
	uprof-report.c \
	uprof-dbus-private.h \
	uprof-dbus.c \
	$(public_h_source)

libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_LIBADD = -lrt
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_LDFLAGS = \
	-export-dynamic \
	@UPROF_DEP_LIBS@
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_CFLAGS = \
	@EXTRA_CFLAGS@ \
	@UPROF_DEP_CFLAGS@ \
	-D_POSIX_C_SOURCE=199309L \
	-D_XOPEN_SOURCE=600
libuprof_@UPROF_MAJOR_VERSION@_@UPROF_MINOR_VERSION@_la_CPPFLAGS = \
    	-DSHARE_DIR=\"$(pkgdatadir)\" \
    	-DUPROF_LOCALEDIR=\""$(uproflocaledir)"\" \
    	-DUPROF_BIN_DIR=\""$(bindir)"\" \
    	-I$(top_srcdir) \
    	-I$(top_srcdir)/uprof \
    	@EXTRA_CPPFLAGS@

if HAVE_NCURSES

bin_PROGRAMS = uprof-tool
uprof_tool_SOURCES = uprof-tool.c
uprof_tool_LDFLAGS = @UPROF_DEP_LIBS@ -luprof-@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@ -lncursesw
uprof_tool_CFLAGS = @EXTRA_CFLAGS@ @UPROF_DEP_CFLAGS@
uprof_tool_CPPFLAGS = @EXTRA_CPPFLAGS@

uprof-service-glue.h: org.freedesktop.UProf.Service.xml Makefile.am
	$(AM_V_GEN)dbus-binding-tool --prefix=_uprof_service --mode=glib-server --output=uprof-service-glue.h $(srcdir)/org.freedesktop.UProf.Service.xml

uprof-reportable-glue.h: org.freedesktop.UProf.Reportable.xml Makefile.am
	$(AM_V_GEN)dbus-binding-tool --prefix=_uprof_report --mode=glib-server --output=uprof-reportable-glue.h $(srcdir)/org.freedesktop.UProf.Reportable.xml

#example-service-glue.h: example-service.xml Makefile.am
#	libtool --mode=execute dbus-binding-tool --prefix=some_object --mode=glib-server --output=example-service-glue.h $(srcdir)/example-service.xml

endif

BUILT_SOURCES = uprof-service-glue.h uprof-reportable-glue.h

uprofincludedir = $(includedir)/uprof-@UPROF_MAJOR_VERSION@.@UPROF_MINOR_VERSION@
uprofinclude_HEADERS = $(public_h_source)

#EXTRA_DIST =

