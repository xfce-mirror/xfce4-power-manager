if HAVE_LIBXFCE4PANEL

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-power-manager-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# xfce4 battery plugin
#
plugin_LTLIBRARIES = \
	libxfce4powermanager.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libxfce4powermanager_la_SOURCES = \
	power-manager-plugin.c      \
	power-manager-plugin.h      \
	power-manager-dialog.c      \
	power-manager-dialog.h      \
	power-manager-button.c      \
	power-manager-button.h      \
	power-manager-config.c      \
	power-manager-config.h      \
	scalemenuitem.c       \
	scalemenuitem.h

#	xfce/xfce-power-manager-plugin.c

libxfce4powermanager_la_CFLAGS =         \
	-I$(top_srcdir)	                \
	-I$(top_srcdir)/src             \
	-DLOCALEDIR=\"$(localedir)\"    \
	$(LIBXFCE4PANEL_CFLAGS)         \
	$(LIBXFCE4UTIL_CFLAGS)          \
	$(LIBXFCE4UI_CFLAGS)            \
	$(XFCONF_CFLAGS)                \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(PLATFORM_CPPFLAGS)            \
	$(PLATFORM_CFLAGS)              \
	$(UPOWER_CFLAGS)

libxfce4powermanager_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libxfce4powermanager_la_LIBADD =                     \
	$(top_builddir)/common/libxfpmcommon.la \
	$(LIBXFCE4PANEL_LIBS)                   \
	$(LIBXFCE4UI_LIBS)                      \
	$(LIBXFCE4UTIL_LIBS)                    \
	$(XFCONF_LIBS)                          \
	$(GTK_LIBS)                             \
	$(GLIB_LIBS)                            \
	$(UPOWER_LIBS)

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = power-manager-plugin.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =                                                            \
	$(desktop_in_files)

CLEANFILES =                                                            \
	$(desktop_DATA)

endif
