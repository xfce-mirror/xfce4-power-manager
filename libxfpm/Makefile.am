noinst_LTLIBRARIES = 			\
	libxfpmhal.la			\
	libxfpmhal_iface.la		\
	libxfpmcommon.la

libxfpmhal_la_SOURCES =			\
	xfpm-string.c			\
	xfpm-string.h			\
	hal-marshal.c			\
	hal-marshal.h			\
	hal-proxy.c			\
	hal-proxy.h			\
	hal-manager.c			\
	hal-manager.h			\
	hal-power.c			\
	hal-power.h			\
	hal-device.c			\
	hal-device.h			\
	hal-battery.c			\
	hal-battery.h			\
	hal-enum.h

libxfpmhal_la_CFLAGS =			\
	$(GLIB_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)

libxfpmhal_iface_la_SOURCES =		\
	hal-iface.c			\
	hal-iface.h			\
	xfpm-string.c			\
	xfpm-string.h

libxfpmhal_iface_la_CFLAGS =		\
	$(GLIB_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)

libxfpmcommon_la_SOURCES =      	\
        xfpm-string.c           	\
        xfpm-string.h           	\
        xfpm-dbus.c             	\
        xfpm-dbus.h             	\
        xfpm-popups.c           	\
        xfpm-popups.h           	\
        xfpm-common.c           	\
        xfpm-common.h

libxfpmcommon_la_CFLAGS =       	\
        $(GTK_CFLAGS)           	\
        $(GLIB_CFLAGS)          	\
        $(DBUS_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)

if MAINTAINER_MODE

BUILT_SOURCES =				\
	hal-marshal.c			\
	hal-marshal.h

hal-marshal.h: hal-marshal.list
	glib-genmarshal --header --prefix="_hal_marshal" \
            hal-marshal.list > tmp-marshal.h && \
        cp tmp-marshal.h $(@F) && \
        rm -f tmp-marshal.h tmp-marshal.h~

hal-marshal.c: hal-marshal.h
	glib-genmarshal --body --prefix="_hal_marshal" \
            hal-marshal.list > tmp-marshal.c && \
        cp tmp-marshal.c $(@F) && \
        rm -f tmp-marshal.c tmp-marshal.c~

endif
