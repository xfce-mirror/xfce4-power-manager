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
	-I$(top_srcdir) 	        \
	-DSBINDIR=\"$(sbindir)\"		\
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(LIBXFCE4UTIL_CFLAGS)
