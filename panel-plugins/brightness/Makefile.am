plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-brightness-plugin

xfce4_brightness_plugin_SOURCES =		\
	brightness-plugin.c			\
	brightness-button.c			\
	brightness-button.h			\
	brightness-proxy.c			\
	brightness-proxy.h

xfce4_brightness_plugin_CFLAGS =		\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/src			\
	-DLOCALEDIR=\"$(localedir)\"  		\
	$(LIBXFCE4PANEL_CFLAGS)       		\
	$(LIBXFCE4UTIL_CFLAGS)        		\
	$(GTK_CFLAGS)			      	\
	$(GLIB_CFLAGS)				\
	$(DBUS_GLIB_CFLAGS)

xfce4_brightness_plugin_LDADD =			\
	$(top_builddir)/libxfpm/libxfpmhal.la	\
	$(top_builddir)/libxfpm/libxfpmcommon.la\
	$(LIBXFCE4PANEL_LIBS)   		\
	$(LIBXFCE4UTIL_LIBS)			\
	$(GTK_LIBS)   				\
	$(GLIB_LIBS)				\
	$(DBUS_GLIB_LIBS)

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = xfce4-brightness-plugin.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =                                                            \
        $(desktop_in_in_files)

CLEANFILES =                                                            \
        $(desktop_in_files)                                             \
        $(desktop_DATA)

%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
