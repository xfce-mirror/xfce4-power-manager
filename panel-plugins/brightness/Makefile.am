plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-brightness-plugin

xfce4_brightness_plugin_SOURCES =							\
									main.c
	
xfce4_brightness_plugin_CFLAGS =							      \
									-DLOCALEDIR=\"$(localedir)\"  \
									$(LIBXFCE4PANEL_CFLAGS)       \
									$(LIBXFCE4UTIL_CFLAGS)        \
									$(GTK_CFLAGS)			      \
									$(GLIB_CFLAGS)			      \
									$(HAL_CFLAGS)

xfce4_brightness_plugin_LDADD =								\
									$(LIBXFCE4PANEL_LIBS)   \
									$(LIBXFCE4UTIL_LIBS)	\
									$(GTK_LIBS)   			\
									$(GLIB_LIBS)			\
									$(HAL_LIBS)

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
