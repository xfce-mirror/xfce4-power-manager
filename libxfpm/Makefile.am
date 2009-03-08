noinst_LTLIBRARIES = 			\
	libxfpmhal.la			\
	libxfpmdbus.la			\
	libxfpmcommon.la

libxfpmhal_la_SOURCES =			\
	hal-ctx.c			\
	hal-ctx.h			\
	hal-power.c			\
	hal-power.h			\
	hal-device.c			\
	hal-device.h

libxfpmhal_la_CFLAGS =			\
	$(HAL_CFLAGS)			\
	$(GLIB_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)

libxfpmdbus_la_SOURCES =		\
	dbus-hal.c			\
	dbus-hal.h


libxfpmdbus_la_CFLAGS =			\
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