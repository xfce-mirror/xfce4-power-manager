plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-inhibit-plugin

xfce4_inhibit_plugin_SOURCES =			\
	inhibit-plugin.c			\
	inhibit-client.h

xfce4_inhibit_plugin_CFLAGS =			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/src			\
	-DLOCALEDIR=\"$(localedir)\"  		\
	$(LIBXFCE4PANEL_CFLAGS)       		\
	$(LIBXFCE4UTIL_CFLAGS)        		\
	$(LIBNOTIFY_CFLAGS)			\
	$(GTK_CFLAGS)			      	\
	$(GLIB_CFLAGS)				\
	$(DBUS_GLIB_CFLAGS)

xfce4_inhibit_plugin_LDADD =			\
	$(top_builddir)/libxfpm/libxfpmcommon.la\
	$(LIBXFCE4PANEL_LIBS)   		\
	$(LIBXFCE4UTIL_LIBS)			\
	$(LIBNOTIFY_LIBS)			\
	$(GTK_LIBS)   				\
	$(GLIB_LIBS)				\
	$(DBUS_GLIB_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES =					\
	inhibit-client.h

inhibit-client.h: $(top_srcdir)/src/org.freedesktop.PowerManagement.Inhibit.xml
	dbus-binding-tool --mode=glib-client --prefix=inhibit_plugin $< >$@

endif
#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = xfce4-inhibit-plugin.desktop.in.in
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

DISTCLEANFILES =		\
	$(BUILT_SOURCES)