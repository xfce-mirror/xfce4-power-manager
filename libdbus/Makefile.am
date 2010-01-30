noinst_LTLIBRARIES = 			\
	libxfpmdbus.la

libxfpmdbus_la_SOURCES =		\
	xfpm-dbus.c			\
	xfpm-dbus.h			\
	xfpm-dbus-monitor.c		\
	xfpm-dbus-monitor.h		\
	xfpm-unique.c			\
	xfpm-unique.h			\
	xfpm-dbus-marshal.c		\
	xfpm-dbus-marshal.h		\
	org.xfce.unique.h
	
libxfpmdbus_la_CFLAGS =			\
	$(GLIB_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)		\
	$(DBUS_GLIB_CFLAGS)

if MAINTAINER_MODE

BUILT_SOURCES =				\
	xfpm-dbus-marshal.c		\
	xfpm-dbus-marshal.h		\
	org.xfce.unique.h

xfpm-dbus-marshal.c: xfpm-dbus-marshal.list
	echo "#include \"xfpm-dbus-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_xfpm_dbus_marshal --body >> $@

xfpm-dbus-marshal.h: xfpm-dbus-marshal.list
	glib-genmarshal $< --prefix=_xfpm_dbus_marshal --header > $@

org.xfce.unique.h: org.xfce.unique.xml
	dbus-binding-tool --mode=glib-server --prefix=xfce_unique $< >$@

endif

EXTRA_DIST =				\
	xfpm-dbus-marshal.list

DISTCLEANFILES =			\
	$(BUILT_SOURCES)