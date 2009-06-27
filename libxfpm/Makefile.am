noinst_LTLIBRARIES = 			\
	libxfpmhal.la			\
	libxfpmhalpower.la		\
	libxfpmcommon.la

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

libxfpmhalpower_la_SOURCES =		\
	hal-power.c			\
	hal-power.h			\
	hal-battery.c			\
	hal-battery.h			\
	hal-enum.h

libxfpmhalpower_la_CFLAGS  =		\
	$(GLIB_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)

libxfpmcommon_la_SOURCES =      	\
        xfpm-string.c           	\
        xfpm-string.h           	\
        xfpm-dbus.c             	\
        xfpm-dbus.h             	\
        xfpm-popups.c           	\
        xfpm-popups.h           	\
        xfpm-common.c           	\
        xfpm-common.h			\
	xfpm-notify.c			\
	xfpm-notify.h

libxfpmcommon_la_CFLAGS =       	\
        $(GTK_CFLAGS)           	\
        $(GLIB_CFLAGS)          	\
        $(DBUS_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)

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