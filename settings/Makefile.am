bin_PROGRAMS = xfce4-power-manager-settings

xfce4_power_manager_settings_SOURCES = 				\
	xfce-power-manager-dbus.h			\
	xfce-power-manager-dbus.c			\
	xfpm-settings-main.c					\
	xfpm-settings-app.c					\
	xfpm-settings-app.h					\
	xfpm-settings.c						\
	xfpm-settings.h						\
	$(top_srcdir)/common/xfpm-config.h				\
	$(top_srcdir)/common/xfpm-enum.h				\
	$(top_srcdir)/common/xfpm-enum-glib.h \
	$(top_srcdir)/data/interfaces/xfpm-resources.c \
	$(NULL)

xfce4_power_manager_settings_CFLAGS =				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/common					\
	-I$(top_srcdir)/src					\
	-I$(top_srcdir)/data					\
	-I$(top_srcdir)/libdbus                 		\
	-DLOCALEDIR=\"$(localedir)\"				\
	-DG_LOG_DOMAIN=\"xfce4-power-manager-settings\" 	\
	$(MODULE_FLAGS)						\
	$(GTK_CFLAGS)						\
	$(GLIB_CFLAGS)						\
	$(GIO_CFLAGS)						\
	$(GIO_UNIX_CFLAGS)						\
	$(GTHREAD_CFLAGS)					\
	$(GMODULE_CFLAGS)					\
	$(LIBXFCE4UI_CFLAGS)					\
	$(LIBXFCE4UTIL_CFLAGS)					\
	$(XFCONF_CFLAGS)					\
	$(UPOWER_CFLAGS)

xfce4_power_manager_settings_LDFLAGS = -export-dynamic

xfce4_power_manager_settings_LDADD   =				\
	$(top_builddir)/libdbus/libxfpmdbus.la  		\
	$(top_builddir)/common/libxfpmcommon.la			\
	$(GTK_LIBS)						\
	$(GLIB_LIBS)						\
	$(GIO_LIBS)						\
	$(GIO_UNIX_LIBS)						\
	$(GTHREAD_LIBS)						\
	$(GMODULE_LIBS)						\
	$(LIBXFCE4UI_LIBS)					\
	$(LIBXFCE4UTIL_LIBS)					\
	$(XFCONF_LIBS)						\
	$(UPOWER_LIBS)


manpagedir = $(mandir)/man1

manpage_DATA = xfce4-power-manager-settings.1

BUILT_SOURCES = 			\
	xfce-power-manager-dbus.c	\
	xfce-power-manager-dbus.h

xfce-power-manager-dbus.c xfce-power-manager-dbus.h: $(top_srcdir)/src/org.xfce.Power.Manager.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) --interface-prefix=org.xfce. --c-namespace=Xfpm --generate-c-code=xfce-power-manager-dbus $<

#
# .desktop file
#
desktop_in_files = xfce4-power-manager-settings.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA =	$(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = 					\
	$(desktop_in_files)			\
	$(manpage_DATA) \
	$(top_srcdir)/src/org.xfce.Power.Manager.xml \
	meson.build \
	$(NULL)

DISTCLEANFILES =				\
	$(BUILT_SOURCES)			\
	$(desktop_DATA)
