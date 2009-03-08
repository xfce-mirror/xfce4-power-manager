bin_PROGRAMS = xfce4-power-manager-settings

xfce4_power_manager_settings_SOURCES = 				\
	xfce-power-manager-dbus-client.h			\
	xfpm-settings_glade.h					\
	xfpm-settings-main.c					\
	xfpm-settings.c						\
	xfpm-settings.h						\
	$(top_srcdir)/src/xfpm-config.h


xfce4_power_manager_settings_CFLAGS =				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/src					\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DG_LOG_DOMAIN=\"xfce4-power-manager-settings\" 	\
	$(GTK_CFLAGS)						\
	$(GLIB_CFLAGS)						\
	$(DBUS_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	$(LIBXFCE4GUI_CFLAGS)					\
	$(LIBXFCE4UTIL_CFLAGS)					\
	$(LIBGLADE_CFLAGS)					\
	$(XFCONF_CFLAGS)

xfce4_power_manager_settings_LDADD   =				\
	$(GTK_LIBS)						\
	$(GLIB_LIBS)						\
	$(DBUS_LIBS)						\
	$(DBUS_GLIB_LIBS)					\
	$(LIBXFCE4GUI_LIBS)					\
	$(LIBXFCE4UTIL_LIBS)					\
	$(LIBGLADE_LIBS)					\
	$(XFCONF_LIBS)						\
	$(top_builddir)/libxfpm/libxfpmcommon.la


if MAINTAINER_MODE

BUILT_SOURCES = 				\
	xfce-power-manager-dbus-client.h	\
	xfpm-settings_glade.h

xfce-power-manager-dbus-client.h: $(top_srcdir)/src/org.xfce.Power.Manager.xml
	dbus-binding-tool --mode=glib-client --prefix=xfpm_manager $< >$@

xfpm-settings_glade.h: $(srcdir)/xfpm-settings.glade
	exo-csource --static --name=xfpm_settings_glade $< >$@

endif

#
# .desktop file
#
desktop_in_files = xfce4-power-manager.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA =	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 					\
	xfce-power-manaher-dbus-client.h	\
	xfpm-settings_glade.h			\
	$(desktop_in_files)

DISTCLEANFILES =				\
	$(BUILT_SOURCES)			\
	$(desktop_DATA)