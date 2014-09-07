AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"lxde-power-manager-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)


#
# lxde power manager plugin
#

plugin_LTLIBRARIES =   \
	lxde_power_manager.la

plugindir = \
	$(libdir)/lxpanel/plugins

lxde_power_manager_la_SOURCES =    \
	lxde-power-manager-plugin.c   \
	../power-manager-button.c     \
	../power-manager-button.h     \
	../scalemenuitem.c            \
	../scalemenuitem.h

lxde_power_manager_la_CFLAGS =             \
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

lxde_power_manager_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       $(PLATFORM_LDFLAGS)

lxde_power_manager_la_LIBADD =                     \
	$(top_builddir)/common/libxfpmcommon.la \
	$(LXDEPANEL_LIBS)                       \
	$(LIBXFCE4UI_LIBS)                      \
	$(LIBXFCE4UTIL_LIBS)                    \
	$(XFCONF_LIBS)                          \
	$(GTK_LIBS)                             \
	$(GLIB_LIBS)                            \
	$(DBUS_GLIB_LIBS)                       \
	$(UPOWER_LIBS)
