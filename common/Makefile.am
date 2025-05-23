noinst_LTLIBRARIES =        \
	libxfpmcommon.la

libxfpmcommon_la_SOURCES =  \
	$(BUILT_SOURCES)        \
	xfpm-common.c           \
	xfpm-common.h           \
	xfpm-brightness.c       \
	xfpm-brightness.h       \
	xfpm-debug.c            \
	xfpm-debug.h            \
	xfpm-icons.h            \
	xfpm-power-common.c     \
	xfpm-power-common.h     \
	xfpm-ppd-common.c       \
	xfpm-ppd-common.h       \
	xfpm-enum.h             \
	xfpm-enum-glib.h

libxfpmcommon_la_CFLAGS =           \
	-I$(top_srcdir)                 \
	-DSBINDIR=\"$(sbindir)\"        \
	$(GTK_CFLAGS)                   \
	$(GLIB_CFLAGS)                  \
	$(LIBXFCE4UTIL_CFLAGS)          \
	$(UPOWER_CFLAGS)

libxfpmcommon_la_LIBADD =           \
	-lm                             \
	$(GTK_LIBS)                     \
	$(GLIB_LIBS)                    \
	$(LIBXFCE4UTIL_LIBS)          \
	$(UPOWER_LIBS)

if ENABLE_X11
libxfpmcommon_la_SOURCES += \
	xfpm-brightness-x11.c \
	xfpm-brightness-x11.h

libxfpmcommon_la_CFLAGS += \
	$(LIBX11_CFLAGS) \
	$(XRANDR_CFLAGS)

libxfpmcommon_la_LIBADD += \
	$(LIBX11_LIBS) \
	$(XRANDR_LIBS)
endif

if ENABLE_WAYLAND
libxfpmcommon_la_CFLAGS += \
	$(WAYLAND_CLIENT_CFLAGS)

libxfpmcommon_la_LIBADD += \
	$(top_builddir)/protocols/libprotocols.la \
	$(WAYLAND_CLIENT_LIBS)
endif

if HAVE_POLKIT
libxfpmcommon_la_SOURCES += \
	xfpm-brightness-polkit.c \
	xfpm-brightness-polkit.h

libxfpmcommon_la_CFLAGS += \
	$(POLKIT_CFLAGS)

libxfpmcommon_la_LIBADD += \
	$(POLKIT_LIBS)
endif

xfpm_glib_headers =                \
	$(srcdir)/xfpm-enum-glib.h

BUILT_SOURCES =       \
	xfpm-enum-types.c \
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

EXTRA_DIST = \
	meson.build \
	$(NULL)
