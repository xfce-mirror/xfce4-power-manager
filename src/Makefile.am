bin_PROGRAMS = xfce4-power-manager

xfce4_power_manager_SOURCES = 			\
	xfpm-enum-types.c			\
	xfpm-enum-types.h			\
	xfpm-marshal.c				\
	xfpm-marshal.h				\
	xfce-power-manager-dbus-server.h	\
	xfce-power-manager-dbus-client.h	\
	xfpm-main.c				\
	xfpm-manager.c				\
	xfpm-manager.h				\
	xfpm-engine.c				\
	xfpm-engine.h				\
	xfpm-xfconf.c				\
	xfpm-xfconf.h				\
	xfpm-supply.c				\
	xfpm-supply.h				\
	xfpm-adapter.c				\
	xfpm-adapter.h				\
	xfpm-battery.c				\
	xfpm-battery.h				\
	xfpm-battery-info.c			\
	xfpm-battery-info.h			\
	xfpm-dpms.c				\
	xfpm-dpms.h				\
	xfpm-cpu.c				\
	xfpm-cpu.h				\
	xfpm-button-xf86.c			\
	xfpm-button-xf86.h			\
	xfpm-lid-hal.c				\
	xfpm-lid-hal.h				\
	xfpm-brightness-hal.c			\
	xfpm-brightness-hal.h			\
	xfpm-idle.c				\
	xfpm-idle.h				\
	xfpm-tray-icon.c			\
	xfpm-tray-icon.h			\
	xfpm-notify.c				\
	xfpm-notify.h				\
	xfpm-network-manager.c			\
	xfpm-network-manager.h			\
	xfpm-config.h				\
	xfpm-enum.h


xfce4_power_manager_CFLAGS =			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/libxfpm			\
	-DLOCALEDIR=\"$(localedir)\"		\
	-DG_LOG_DOMAIN=\"xfce4-power-manager\" 	\
	$(GTK_CFLAGS)				\
	$(GLIB_CFLAGS)				\
	$(DBUS_CFLAGS)				\
	$(DBUS_GLIB_CFLAGS)			\
	$(LIBXFCE4GUI_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)			\
	$(XFCONF_CFLAGS)			\
	$(LIBNOTIFY_CFLAGS)			\
	$(DPMS_CFLAGS)              	

xfce4_power_manager_LDADD   =			\
	$(top_builddir)/libxfpm/libxfpmhal.la	\
	$(top_builddir)/libxfpm/libxfpmdbus.la	\
	$(top_builddir)/libxfpm/libxfpmcommon.la\
	$(top_builddir)/libxfpm/libxfpmhal_iface.la\
	$(GTK_LIBS)				\
	$(GLIB_LIBS)				\
	$(DBUS_LIBS)				\
	$(DBUS_GLIB_LIBS)			\
	$(LIBXFCE4GUI_LIBS)			\
	$(LIBXFCE4UTIL_LIBS)			\
	$(XFCONF_LIBS)				\
	$(LIBNOTIFY_LIBS)			\
	$(DPMS_LIBS)		

xfpm_glib_headers = 				\
	$(srcdir)/xfpm-enum-glib.h			

if MAINTAINER_MODE

BUILT_SOURCES = 				\
	xfce-power-manager-dbus-server.h	\
	xfce-power-manager-dbus-client.h	\
	xfpm-marshal.c				\
	xfpm-marshal.h				\
	xfpm-enum-types.c			\
	xfpm-enum-types.h

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

xfpm-marshal.h: $(srcdir)/xfpm-marshal.list
	glib-genmarshal --header --prefix="_xfpm_marshal" \
            $(srcdir)/xfpm-marshal.list > tmp-marshal.h && \
        cp tmp-marshal.h $(@F) && \
        rm -f tmp-marshal.h tmp-marshal.h~

xfpm-marshal.c: xfpm-marshal.h
	glib-genmarshal --body --prefix="_xfpm_marshal" \
            $(srcdir)/xfpm-marshal.list > tmp-marshal.c && \
        cp tmp-marshal.c $(@F) && \
        rm -f tmp-marshal.c tmp-marshal.c~

xfce-power-manager-dbus-server.h: $(srcdir)/org.xfce.Power.Manager.xml
	dbus-binding-tool --mode=glib-server --prefix=xfpm_manager $< >$@

xfce-power-manager-dbus-client.h: $(srdir)org.xfce.Power.Manager.xml
	dbus-binding-tool --mode=glib-client --prefix=xfpm_manager $< >$@

endif

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xfce-power-manager.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST = 					\
	xfpm-marshal.list		\
	xfpm-enum-glib.h		\
	$(autostart_in_files)

DISTCLEANFILES =				\
	$(BUILT_SOURCES)			\
	xfce-power-manager.desktop

