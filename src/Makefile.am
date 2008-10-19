INCLUDES =							\
			$(GTK_CFLAGS)				\
			$(GLIB_CFLAGS)				\
			$(DBUS_CFLAGS)				\
			$(DBUS_GLIB_CFLAGS)			\
			$(HAL_CFLAGS)				\
			$(LIBXFCEGUI_CFLAGS)			\
			$(XFCONF_CFLAGS)			\
			$(LIBNOTIFY_CFLAGS)			\
			$(DPMS_CFLAGS)              \
			-DLOCALEDIR=\"$(localedir)\"


LIBS     =		$(GTK_LIBS)				\
			$(GLIB_LIBS)				\
			$(DBUS_LIBS)				\
			$(DBUS_GLIB_LIBS)			\
			$(LIBXFCEGUI_LIBS)			\
			$(HAL_LIBS)				\
			$(XFCONF_LIBS)				\
			$(LIBNOTIFY_LIBS)			\
			$(DPMS_LIBS)		

bin_PROGRAMS = xfce4-power-manager

generated_files =						\
				xfpm-enum-types.c		\
				xfpm-enum-types.h		\
				xfpm-marshal.c			\
				xfpm-marshal.h
				

xfce4_power_manager_SOURCES = 	$(generated_files)		\
				xfpm-main.c			\
				xfpm-debug.h			\
		      		xfpm-hal.c          		\
				xfpm-hal.h	     		\
				xfpm-driver.c			\
				xfpm-driver.h			\
				xfpm-ac-adapter.c		\
				xfpm-ac-adapter.h		\
				xfpm-cpu.c			\
				xfpm-cpu.h			\
				xfpm-lcd-brightness.c		\
				xfpm-lcd-brightness.h		\
				xfpm-button.c			\
				xfpm-button.h			\
				xfpm-dbus-messages.c		\
				xfpm-dbus-messages.h		\
				xfpm-battery.c			\
				xfpm-battery.h			\
				xfpm-battery-icon.c		\
				xfpm-battery-icon.h		\
				xfpm-spin-button.c		\
				xfpm-spin-button.h		\
				xfpm-dpms.c			\
				xfpm-dpms.h			\
				xfpm-dpms-spins.c		\
				xfpm-dpms-spins.h		\
				xfpm-notify.c			\
				xfpm-notify.h			\
				xfpm-enums.h			\
				xfpm-settings.c			\
				xfpm-settings.h			\
				xfpm-common.c			\
				xfpm-common.h

EXTRA_DIST = 						\
	xfpm-marshal.list				\
	$(generated_files)				\
	$(desktop_in_files)

DISTCLEANFILES =					\
	$(generated_files)				\
	$(desktop_DATA)

xfpm_headers = 						\
	$(srcdir)/xfpm-enums.h			

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
			

			
#
# .desktop file
#
desktopdir = $(datadir)/applications
desktop_in_files =							\
	xfce4-power-manager.desktop.in
desktop_DATA =								\
	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
									
