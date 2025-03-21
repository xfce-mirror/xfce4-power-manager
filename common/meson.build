common_sources = [
  'xfpm-brightness.c',
  'xfpm-brightness.h',
  'xfpm-common.c',
  'xfpm-common.h',
  'xfpm-debug.c',
  'xfpm-debug.h',
  'xfpm-enum-glib.h',
  'xfpm-enum.h',
  'xfpm-icons.h',
  'xfpm-power-common.c',
  'xfpm-power-common.h',
  'xfpm-ppd-common.c',
  'xfpm-ppd-common.h',
]

if enable_x11
  common_sources += [
    'xfpm-brightness-x11.c',
    'xfpm-brightness-x11.h',
  ]
endif

if enable_wayland
  common_sources += wayland_protocols_generated_sources
endif

if polkit.found()
  common_sources += [
    'xfpm-brightness-polkit.c',
    'xfpm-brightness-polkit.h',
  ]
endif

common_sources += gnome.mkenums(
  'xfpm-enum-types.h',
  install_header: false,
  sources: 'xfpm-enum-glib.h',
  fhead: '#ifndef _XFPM_ENUM_TYPES_H\n#define _XFPM_ENUM_TYPES_H\n#include <glib-object.h>\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void);\n#define XFPM_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n\n#endif /* _XFPM_ENUM_TYPES_H__ */',
)
common_sources += gnome.mkenums(
  'xfpm-enum-types.c',
  install_header: false,
  sources: 'xfpm-enum-glib.h',
  fhead: '#include "xfpm-enum-types.h"\n#include "xfpm-enum-glib.h"\n\n',
  fprod: '\n/* enumerations from "@filename@" */\n',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {',
  vprod: '\t{ @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static ("@EnumName@", values);\n  }\n\treturn type;\n}\n',
)

libxfpmcommon = static_library(
  'xfpmcommon',
  common_sources,
  sources: [
    xfpm_resources_c,
    xfce_revision_h,
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('libxfpmcommon'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4util,
    upower_glib,
    x11_deps,
    wayland_deps,
    polkit,
    libm,
  ],
  install: false,
)
