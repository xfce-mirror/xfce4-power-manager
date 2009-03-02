bin_PROGRAMS = xfce4-power-manager		\
	       xfce4-power-manager-settings

generated_files =				\
	xfpm-enum-types.c			\
	xfpm-enum-types.h			\
	xfpm-marshal.c				\
	xfpm-marshal.h


xfce4_power_manager_SOURCES = 			\
	$(generated_files)			\
	xfce-power-manager-dbus-server.h	\
	xfpm-main.c				\
	xfpm-debug.h				\
	xfpm-hal.c  		 		\
	xfpm-hal.h	     			\
	xfpm-driver.c				\
	xfpm-driver.h				\
	xfpm-ac-adapter.c			\
	xfpm-ac-adapter.h			\
	xfpm-cpu.c				\
	xfpm-cpu.h				\
	xfpm-lcd-brightness.c			\
	xfpm-lcd-brightness.h			\
	xfpm-button.c				\
	xfpm-button.h				\
	xfpm-dbus-messages.c			\
	xfpm-dbus-messages.h			\
	xfpm-battery.c				\
	xfpm-battery.h				\
	xfpm-battery-icon.c			\
	xfpm-battery-icon.h			\
	xfpm-dpms.c				\
	xfpm-dpms.h				\
	xfpm-popups.c           		\
	xfpm-popups.h           		\
	xfpm-notify.c				\
	xfpm-notify.h				\
	xfpm-enums.h				\
	xfpm-common.c				\
	xfpm-common.h				\
	xfpm-string.c				\
	xfpm-string.h

xfce4_power_manager_CFLAGS =			\
	-DLOCALEDIR=\"$(localedir)\"		\
	-DG_LOG_DOMAIN=\"xfce4-power-manager\" \
	$(GTK_CFLAGS)				\
	$(GLIB_CFLAGS)				\
	$(DBUS_CFLAGS)				\
	$(DBUS_GLIB_CFLAGS)			\
	$(HAL_CFLAGS)				\
	$(LIBXFCE4GUI_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)			\
	$(XFCONF_CFLAGS)			\
	$(LIBNOTIFY_CFLAGS)			\
	$(DPMS_CFLAGS)              	

xfce4_power_manager_LDADD   =			\
	$(GTK_LIBS)				\
	$(GLIB_LIBS)				\
	$(DBUS_LIBS)				\
	$(DBUS_GLIB_LIBS)			\
	$(LIBXFCE4GUI_LIBS)			\
	$(LIBXFCE4UTIL_LIBS)			\
	$(HAL_LIBS)				\
	$(XFCONF_LIBS)				\
	$(LIBNOTIFY_LIBS)			\
	$(DPMS_LIBS)		


xfce4_power_manager_settings_SOURCES =		\
	xfpm-marshal.c				\
	xfpm-marshal.h				\
	xfpm-settings-main.c			\
	xfpm-settings.c				\
	xfpm-settings.h				\
	xfpm-common.c				\
	xfpm-common.h				\
	xfpm-enum.h				\
	xfpm-enum-types.c			\
	xfpm-enum-types.h			\
	xfpm-string.c				\
	xfpm-string.h				\
	xfpm-spin-button.c			\
	xfpm-spin-button.h			\
	xfpm-dpms-spins.c			\
	xfpm-dpms-spins.h			\
	xfpm-dbus-messages.c			\
	xfpm-dbus-messages.h			\
	xfpm-popups.c				\
	xfpm-popups.h				\
	xfpm-debug.h


xfce4_power_manager_settings_CFLAGS =		\
	-DLOCALEDIR=\"$(localedir)\"		\
	-DG_LOG_DOMAIN=\"xfce4-power-manager-settings\"  \
	$(GTK_CFLAGS)				\
	$(GLIB_CFLAGS)				\
	$(DBUS_CFLAGS)				\
	$(DBUS_GLIB_CFLAGS)			\
	$(LIBXFCE4GUI_CFLAGS)			\
	$(LIBXFCE4UTIL_CFLAGS)			\
	$(XFCONF_CFLAGS)

xfce4_power_manager_settings_LDADD = 		\
	$(GTK_LIBS)				\
	$(GLIB_LIBS)				\
	$(DBUS_LIBS)				\
	$(DBUS_GLIB_LIBS)			\
	$(LIBXFCE4GUI_LIBS)			\
	$(LIBXFCE4UTIL_LIBS)			\
	$(XFCONF_LIBS)

xfpm_headers = 					\
	$(srcdir)/xfpm-enums.h			

if MAINTAINER_MODE

BUILT_SOURCES = 				\
	xfce-power-manager-dbus-server.h	\
	xfce-power-manager-dbus-client.h	\
	xfpm-marshal.c				\
	xfpm-marshal.h				\
	xfpm-enum-types.c			\
	xfpm-enum-types.h

xfpm-enum-types.h: $(xfpm_headers)
	( cd $(srcdir) && glib-mkenums \
		--fhead "#ifndef _XFPM_ENUM_TYPES_H\n#define _XFPM_ENUM_TYPES_H\n#include <glib-object.h>\nG_BEGIN_DECLS\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void);\n#define XFPM_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n#endif /* _XFPM_ENUM_TYPES_H__ */" \
		$(xfpm_headers) ) >> xgen \
	&& (cmp -s xgen xfpm-enum-types.h || cp xgen xfpm-enum-types.h ) \
	&& rm -f xgen xgen~

xfpm-enum-types.c: xfpm-enum-types.h
	( cd $(srcdir) && glib-mkenums \
		--fhead "#include <xfpm-enum-types.h>\n" \
		--fhead "#include \"xfpm-enums.h\"\n\n" \
		--fprod "\n/* enumerations from \"@filename@\" */\n" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {"\
		--vprod "\t{ @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n\treturn type;\n}\n" \
		$(xfpm_headers) ) > xgen \
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
	dbus-binding-tool --mode=glib-server --prefix=xfpm_driver $< >$@

xfce-power-manager-dbus-client.h: $(srdir)org.xfce.Power.Manager.xml
	dbus-binding-tool --mode=glib-client --prefix=xfpm_client $< >$@

endif

#
# .desktop file
#
desktop_in_files = xfce4-power-manager.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA =	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 					\
	xfpm-marshal.list			\
	$(desktop_in_files)

DISTCLEANFILES =				\
	$(BUILT_SOURCES)			\
	$(desktop_DATA)