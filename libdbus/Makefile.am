noinst_LTLIBRARIES = 			\
	libxfpmdbus.la

libxfpmdbus_la_SOURCES =		\
	xfpm-dbus.c			\
	xfpm-dbus.h			\
	xfpm-dbus-monitor.c		\
	xfpm-dbus-monitor.h		\
	xfpm-dbus-marshal.c		\
	xfpm-dbus-marshal.h
	
libxfpmdbus_la_CFLAGS =			\
	$(GLIB_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)

libxfpmdbus_la_LIBADD =			\
	$(GLIB_LIBS)			\
	$(LIBXFCE4UTIL_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES =				\
	xfpm-dbus-marshal.c		\
	xfpm-dbus-marshal.h

xfpm-dbus-marshal.c: xfpm-dbus-marshal.list
	echo "#include \"xfpm-dbus-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_xfpm_dbus_marshal --body >> $@

xfpm-dbus-marshal.h: xfpm-dbus-marshal.list
	glib-genmarshal $< --prefix=_xfpm_dbus_marshal --header > $@

endif

EXTRA_DIST =				\
	xfpm-dbus-marshal.list

DISTCLEANFILES =			\
	$(BUILT_SOURCES)
