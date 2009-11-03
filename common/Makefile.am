noinst_LTLIBRARIES =                    \
        libxfpmcommon.la

libxfpmcommon_la_SOURCES =              \
	xfpm-common.c			\
	xfpm-common.h			\
	xfpm-brightness.c		\
	xfpm-brightness.h		\
	xfpm-debug.c			\
	xfpm-debug.h			\
	xfpm-icons.h

libxfpmcommon_la_CFLAGS =               \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(LIBXFCE4UTIL_CFLAGS)

if ENABLE_HAL

libxfpmcommon_la_CFLAGS += 		\
	$(DBUS_GLIB_CFLAGS)

libxfpmcommon_la_LIBADD =		\
	$(top_builddir)/libhal/libxfpmhal.la
endif