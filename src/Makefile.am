bin_PROGRAMS = xfce4-power-manager		\
	       xfce4-power-information

xfce4_power_manager_SOURCES =                   \
	$(BUILT_SOURCES)			\
	xfpm-enum.h				\
	xfpm-enum-glib.h			\
	xfpm-dbus-service.c			\
	xfpm-dbus-service.h			\
	xfpm-main.c				\
	xfpm-manager.c				\
	xfpm-manager.h				\
	xfpm-power.c				\
	xfpm-power.h				\
	xfpm-power-common.c			\
	xfpm-power-common.h			\
	xfpm-battery.c				\
	xfpm-battery.h				\
	xfpm-xfconf.c				\
	xfpm-xfconf.h				\
	xfpm-disks.c				\
	xfpm-disks.h				\
	xfpm-sm-client.c			\
	xfpm-sm-client.h			\
	egg-idletime.c				\
	egg-idletime.h				\
	xfpm-backlight.c			\
	xfpm-backlight.h			\
	xfpm-dpms.c				\
	xfpm-dpms.h				\
	xfpm-button.c				\
	xfpm-button.h				\
	xfpm-network-manager.c			\
	xfpm-network-manager.h			\
	xfpm-notify.c				\
	xfpm-notify.h				\
	xfpm-polkit.c				\
	xfpm-polkit.h				\
	xfpm-errors.c				\
	xfpm-errors.h				\
	gsd-media-keys-window.c			\
	gsd-media-keys-window.h

xfce4_power_manager_CFLAGS =                    \
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/common                  \
	-I$(top_srcdir)/libdbus                 \
	-DLOCALEDIR=\"$(localedir)\"            \
	-DG_LOG_DOMAIN=\"xfce4-power-manager\"  \
	-DSYSCONFDIR=\"$(sysconfdir)\"		\
	$(GOBJECT_CFLAGS)                       \
	$(GTHREAD_CFLAGS)                       \
	$(DBUS_GLIB_CFLAGS)                     \
	$(LIBXFCE4UI_CFLAGS)                    \
	$(XFCONF_CFLAGS)                        \
	$(LIBNOTIFY_CFLAGS)                     \
	$(XRANDR_CFLAGS)			\
	$(DPMS_CFLAGS)           		\
	$(UPOWER_GLIB_CFLAGS)			\
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
	$(XRANDR_LIBS)				\
	$(UPOWER_GLIB_LIBS)			\
	$(DPMS_LIBS)

xfce4_power_information_SOURCES =		\
	xfpm-power-info.c			\
	xfpm-power-common.c			\
	xfpm-power-common.h

xfce4_power_information_CFLAGS =		\
	-I$(top_srcdir)                         \
	-I$(top_srcdir)/common                  \
	-I$(top_srcdir)/libdbus                 \
	-DLOCALEDIR=\"$(localedir)\"            \
	-DG_LOG_DOMAIN=\"xfce4-power-information\"\
	$(GOBJECT_CFLAGS)                       \
	$(DBUS_GLIB_CFLAGS)                     \
	$(LIBXFCE4UI_CFLAGS)			\
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

xfce4_power_information_LDFLAGS =		\
	-no-undefined				\
	$(PLATFORM_LDFLAGS)

xfce4_power_information_LDADD =			\
	$(GOBJECT_LIBS)                         \
	$(DBUS_GLIB_LIBS)                       \
	$(LIBXFCE4UI_LIBS)			\
	$(top_builddir)/libdbus/libxfpmdbus.la

if ENABLE_POLKIT

sbin_PROGRAMS = xfpm-power-backlight-helper 

xfpm_power_backlight_helper_SOURCES =           \
       xfpm-backlight-helper.c

xfpm_power_backlight_helper_LDADD =             \
       $(GLIB_LIBS)                             \
       -lm

xfpm_power_backlight_helper_CFLAGS =            \
        $(GLIB_CFLAGS)                          \
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA =					\
	org.xfce.power.policy

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

.in2.in:
	sed "s|[@]sbindir@|${sbindir}|" $< > $@

endif

manpagedir = $(mandir)/man1

manpage_DATA = xfce4-power-manager.1

xfpm_glib_headers =                             \
        $(srcdir)/xfpm-enum-glib.h

BUILT_SOURCES =					\
	xfpm-dbus-service-infos.h		\
	xfpm-enum-types.c                       \
	xfpm-enum-types.h			\
	xfpm-marshal.c                          \
        xfpm-marshal.h

if MAINTAINER_MODE

xfpm-enum-types.h: $(xfpm_glib_headers)
	( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef _XFPM_ENUM_TYPES_H\n#define _XFPM_ENUM_TYPES_H\n#include <glib-object.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void);\n#define XFPM_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* _XFPM_ENUM_TYPES_H__ */" \
		$(xfpm_glib_headers) ) >> xgen \
	&& (cmp -s xgen xfpm-enum-types.h || cp xgen xfpm-enum-types.h ) \
	&& rm -f xgen xgen~

xfpm-enum-types.c: xfpm-enum-types.h
	( cd $(srcdir) && glib-mkenums \
		--fhead "#include <xfpm-enum-types.h>\n" \
		--fhead "#include \"xfpm-enum-glib.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */\n" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		$(xfpm_glib_headers) ) > xgen \
	&& cp xgen xfpm-enum-types.c  \
	&& rm -f xgen xgen~

xfpm-marshal.c: xfpm-marshal.list
	echo "#include \"xfpm-marshal.h\"" > $@ && \
	glib-genmarshal $< --prefix=_xfpm_marshal --body >> $@

xfpm-marshal.h: xfpm-marshal.list
	glib-genmarshal $< --prefix=_xfpm_marshal --header > $@

xfpm-dbus-service-infos.h: $(srcdir)/xfpm-dbus-service-infos.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --prefix=xfpm_dbus_service --mode=glib-server $< >$@

endif

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xfce4-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST =                                    \
	xfpm-marshal.list			\
        $(autostart_in_files)                   \
        org.xfce.power.policy.in2              \
        $(manpage_DATA)

DISTCLEANFILES =                                \
        $(BUILT_SOURCES)                        \
        xfce4-power-manager.desktop             \
        org.xfce.power.policy                  \
        org.xfce.power.policy.in
