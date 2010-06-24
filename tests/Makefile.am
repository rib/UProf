
noinst_PROGRAMS = simple suspend suspend2 suspend3 dlopen recursion linking sanity_check custom-attributes dbus-service

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
custom_attributes_SOURCES = custom-attributes.c
suspend_SOURCES = suspend.c
suspend2_SOURCES = suspend2.c
suspend3_SOURCES = suspend3.c
linking_SOURCES = linking.c
dlopen_SOURCES = dlopen.c
dlopen_LDFLAGS = \
		 -export-dynamic \
		 $(AM_LDFLAGS)

dlopen_LDADD = -ldl
dbus_service_SOURCES = dbus-service.c

all-local: module.so
include ./$(DEPDIR)/module.Po
module.so: module.c
	$(CC) -c module.c -MT module.o -MD -MP -MF .deps/module.Tpo -fPIC $(AM_CFLAGS)
	$(CC) -o module.so -shared module.o
clean-local:
	rm -f module.so
