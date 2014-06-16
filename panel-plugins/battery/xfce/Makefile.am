AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-battery-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# xfce4 battery plugin
#
plugin_LTLIBRARIES = \
	libxfce4battery.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libxfce4battery_la_SOURCES = \
	battery-plugin.c         \
	../battery-button.c      \
	../battery-button.h

libxfce4battery_la_CFLAGS =         \
	-I$(top_srcdir)	                \
	-I$(top_srcdir)/src             \
	-DLOCALEDIR=\"$(localedir)\"    \
	-DXFCE_PLUGIN                   \
	$(LIBXFCE4PANEL_CFLAGS)         \
	$(LIBXFCE4UTIL_CFLAGS)          \
	$(LIBXFCE4UI_CFLAGS)            \
	$(XFCONF_CFLAGS)                \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(PLATFORM_CPPFLAGS)            \
	$(PLATFORM_CFLAGS)              \
	$(DBUS_GLIB_CFLAGS)             \
	$(UPOWER_CFLAGS)

libxfce4battery_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libxfce4battery_la_LIBADD =                     \
	$(top_builddir)/common/libxfpmcommon.la \
	$(LIBXFCE4PANEL_LIBS)                   \
	$(LIBXFCE4UI_LIBS)                      \
	$(LIBXFCE4UTIL_LIBS)                    \
	$(XFCONF_LIBS)                          \
	$(GTK_LIBS)                             \
	$(GLIB_LIBS)                            \
	$(DBUS_GLIB_LIBS)                       \
	$(UPOWER_LIBS)

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = xfce4-battery-plugin.desktop.in.in
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
