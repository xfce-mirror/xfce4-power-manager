bin_PROGRAMS = xfce4-power-manager

xfce4_power_manager_SOURCES =                   \
	$(BUILT_SOURCES)			\
	xfpm-main.c				\
	xfpm-manager.c				\
	xfpm-manager.h				\
	xfpm-power.c				\
	xfpm-power.h				\
	xfpm-battery.c				\
	xfpm-battery.h				\
	xfpm-xfconf.c				\
	xfpm-xfconf.h				\
	xfpm-console-kit.c			\
	xfpm-console-kit.h			\
	xfpm-systemd.c				\
	xfpm-systemd.h				\
	egg-idletime.c				\
	egg-idletime.h				\
	xfpm-backlight.c			\
	xfpm-backlight.h			\
	xfpm-kbd-backlight.c			\
	xfpm-kbd-backlight.h			\
	xfpm-dpms.c				\
	xfpm-dpms.h				\
	xfpm-button.c				\
	xfpm-button.h				\
	xfpm-network-manager.c			\
	xfpm-network-manager.h			\
	xfpm-inhibit.c				\
	xfpm-inhibit.h				\
	xfpm-notify.c				\
	xfpm-notify.h				\
	xfpm-polkit.c				\
	xfpm-polkit.h				\
	xfpm-errors.c				\
	xfpm-errors.h				\
	xfpm-suspend.c				\
	xfpm-suspend.h				\
	../panel-plugins/power-manager-plugin/power-manager-button.c	\
	../panel-plugins/power-manager-plugin/power-manager-button.h	\
	../panel-plugins/power-manager-plugin/scalemenuitem.c			\
	../panel-plugins/power-manager-plugin/scalemenuitem.h

xfce4_power_manager_CFLAGS =                    \
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/common                  \
	-I$(top_srcdir)/libdbus                 \
	-DLOCALEDIR=\"$(localedir)\"            \
	-DG_LOG_DOMAIN=\"xfce4-power-manager\"  \
	-DSYSCONFDIR=\"$(sysconfdir)\"          \
	-DSBINDIR=\"$(sbindir)\"                \
	-DUPOWER_ENABLE_DEPRECATED              \
	-DXFPM_SUSPEND_HELPER_CMD=\"$(prefix)/bin/xfce4-pm-helper\" \
	-UXFCE_PLUGIN                           \
	-DXFPM_SYSTRAY							\
	$(GOBJECT_CFLAGS)                       \
	$(GTHREAD_CFLAGS)                       \
	$(DBUS_GLIB_CFLAGS)                     \
	$(LIBXFCE4UI_CFLAGS)                    \
	$(XFCONF_CFLAGS)                        \
	$(LIBNOTIFY_CFLAGS)                     \
	$(UPOWER_CFLAGS)                        \
	$(XRANDR_CFLAGS)			\
	$(DPMS_CFLAGS)           		\
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

xfce4_power_manager_LDFLAGS =			\
	-no-undefined				\
	$(PLATFORM_LDFLAGS)

xfce4_power_manager_LDADD =                     \
	$(top_builddir)/common/libxfpmcommon.la \
	$(top_builddir)/libdbus/libxfpmdbus.la  \
	$(GOBJECT_LIBS)                         \
	$(GTHREAD_LIBS)                         \
	$(DBUS_GLIB_LIBS)                       \
	$(LIBXFCE4UI_LIBS)                      \
	$(XFCONF_LIBS)                          \
	$(LIBNOTIFY_LIBS)                       \
	$(UPOWER_LIBS)                          \
	$(XRANDR_LIBS)				\
	$(DPMS_LIBS)

if ENABLE_POLKIT

sbin_PROGRAMS = xfpm-power-backlight-helper     \
	xfce4-pm-helper

xfpm_power_backlight_helper_SOURCES =           \
	xfpm-backlight-helper.c

xfpm_power_backlight_helper_LDADD =             \
	$(GLIB_LIBS)                             \
	-lm

xfpm_power_backlight_helper_CFLAGS =            \
        $(GLIB_CFLAGS)                          \
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

xfce4_pm_helper_SOURCES =  \
	xfpm-pm-helper.c

xfce4_pm_helper_CFLAGS =   \
	-I$(top_srcdir)        \
	$(GLIB_CFLAGS)         \
	$(PLATFORM_CPPFLAGS)   \
	$(PLATFORM_CFLAGS)     \
	$(LIBXFCE4UTIL_CFLAGS)

xfce4_pm_helper_LDFLAGS = \
	$(LIBXFCE4UTIL_LIBS)  \
	$(GLIB_LIBS)

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA =					\
	org.xfce.power.policy

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

%.policy.in: %.policy.in.in
	sed "s|[@]sbindir@|${sbindir}|" $< > $@

endif

manpagedir = $(mandir)/man1

manpage_DATA = xfce4-power-manager.1

BUILT_SOURCES =					\
	xfce-power-manager-dbus.c		\
	xfce-power-manager-dbus.h		\
	xfpm-marshal.c                          \
	xfpm-marshal.h				\
	org.freedesktop.PowerManagement.c       \
	org.freedesktop.PowerManagement.h       \
	org.freedesktop.PowerManagement.Inhibit.c \
	org.freedesktop.PowerManagement.Inhibit.h

if MAINTAINER_MODE

xfpm-marshal.c: xfpm-marshal.list
	echo "#include \"xfpm-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_xfpm_marshal --body >> $@

xfpm-marshal.h: xfpm-marshal.list
	glib-genmarshal $< --prefix=_xfpm_marshal --header > $@

xfce-power-manager-dbus.c xfce-power-manager-dbus.h: $(srcdir)/org.xfce.Power.Manager.xml
	gdbus-codegen --interface-prefix=org.xfce. --c-namespace=Xfpm --generate-c-code=xfce-power-manager-dbus $<

org.freedesktop.PowerManagement.c org.freedesktop.PowerManagement.h: $(srcdir)/org.freedesktop.PowerManagement.xml
	gdbus-codegen --interface-prefix=org.freedesktop. --c-namespace=Xfpm --generate-c-code=org.freedesktop.PowerManagement $<

org.freedesktop.PowerManagement.Inhibit.c org.freedesktop.PowerManagement.Inhibit.h: $(srcdir)/org.freedesktop.PowerManagement.Inhibit.xml
	gdbus-codegen --interface-prefix=org.freedesktop. --c-namespace=Xfpm --generate-c-code=org.freedesktop.PowerManagement.Inhibit $<

endif

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xfce4-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST =                                    \
	xfpm-marshal.list			\
	$(autostart_in_files)                   \
	org.xfce.power.policy.in.in              \
	$(manpage_DATA)

DISTCLEANFILES =                                \
	xfce4-power-manager.desktop             \
	org.xfce.power.policy                  \
	org.xfce.power.policy.in

MAINTAINERCLEANFILES =                                \
	$(BUILT_SOURCES)
