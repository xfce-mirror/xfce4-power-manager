plugindir = $(libdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-brightness-plugin

xfce4_brightness_plugin_SOURCES =		\
	brightness-plugin.c			\
	brightness-button.c			\
	brightness-button.h

xfce4_brightness_plugin_CFLAGS =		\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/src			\
	-DLOCALEDIR=\"$(localedir)\"  		\
	$(LIBXFCE4PANEL_CFLAGS)       		\
	$(LIBXFCE4UTIL_CFLAGS)        		\
	$(LIBXFCE4UI_CFLAGS)			\
	$(GTK_CFLAGS)			      	\
	$(GLIB_CFLAGS)				\
	$(XRANDR_CFLAGS)			\
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

xfce4_brightness_plugin_LDFLAGS =		\
	-no-undefined				\
	$(PLATFORM_LDFLAGS)

xfce4_brightness_plugin_LDADD =			\
	$(top_builddir)/common/libxfpmcommon.la \
	$(LIBXFCE4PANEL_LIBS)   		\
	$(LIBXFCE4UI_LIBS)			\
	$(LIBXFCE4UTIL_LIBS)			\
	$(GTK_LIBS)   				\
	$(GLIB_LIBS)				\
	$(XRANDR_LIBS)

if ENABLE_HAL

xfce4_brightness_plugin_CFLAGS += 		\
	$(DBUS_GLIB_CFLAGS)

xfce4_brightness_plugin_LDADD += 		\
	$(DBUS_GLIB_LIBS)

endif

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
	sed -e "s,\@libdir\@,$(libdir),g" < $< > $@
