AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"lxde-battery-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)


#
# lxde battery plugin
#

plugin_LTLIBRARIES =   \
	lxdebattery.la

plugindir = \
	$(libdir)/lxpanel/plugins

lxdebattery_la_SOURCES =    \
	battery-plugin.c        \
	../battery-button.c     \
	../battery-button.h

lxdebattery_la_CFLAGS =             \
	-I$(top_srcdir)	                \
	-I$(top_srcdir)/src             \
	-DLOCALEDIR=\"$(localedir)\"    \
	-DLXDE_PLUGIN                   \
	$(LXDEPANEL_CFLAGS)             \
	$(LIBXFCE4UTIL_CFLAGS)          \
	$(LIBXFCE4UI_CFLAGS)            \
	$(XFCONF_CFLAGS)                \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(PLATFORM_CPPFLAGS)            \
	$(PLATFORM_CFLAGS)              \
	$(DBUS_GLIB_CFLAGS)             \
	$(UPOWER_CFLAGS)

lxdebattery_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       $(PLATFORM_LDFLAGS)

lxdebattery_la_LIBADD =                     \
	$(top_builddir)/common/libxfpmcommon.la \
	$(LXDEPANEL_LIBS)                       \
	$(LIBXFCE4UI_LIBS)                      \
	$(LIBXFCE4UTIL_LIBS)                    \
	$(XFCONF_LIBS)                          \
	$(GTK_LIBS)                             \
	$(GLIB_LIBS)                            \
	$(DBUS_GLIB_LIBS)                       \
	$(UPOWER_LIBS)
