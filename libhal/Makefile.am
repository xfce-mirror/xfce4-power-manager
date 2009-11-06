noinst_LTLIBRARIES = 			\
	libxfpmhal.la

libxfpmhal_la_SOURCES =			\
	hal-manager.c			\
	hal-manager.h			\
	hal-device.c			\
	hal-device.h			\
	hal-marshal.c			\
	hal-marshal.h

libxfpmhal_la_CFLAGS =			\
	$(GLIB_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)

libxfpmhal_la_LIBADD =			\
	$(top_builddir)/libdbus/libxfpmdbus.la

if MAINTAINER_MODE

BUILT_SOURCES =				\
	hal-marshal.c			\
	hal-marshal.h

hal-marshal.c: hal-marshal.list
	echo "#include \"hal-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_hal_marshal --body >> $@

hal-marshal.h: hal-marshal.list
	glib-genmarshal $< --prefix=_hal_marshal --header > $@

endif

EXTRA_DIST =				\
	hal-marshal.list

DISTCLEANFILES =			\
	$(BUILT_SOURCES)