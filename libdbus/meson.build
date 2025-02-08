dbus_sources = [
  'xfpm-dbus-monitor.c',
  'xfpm-dbus-monitor.h',
  'xfpm-dbus.c',
  'xfpm-dbus.h',
]

dbus_sources += gnome.genmarshal(
  'xfpm-dbus-marshal',
  sources: 'xfpm-dbus-marshal.list',
  prefix: '_xfpm_dbus_marshal',
  internal: true,
  install_header: false,
)

libxfpmdbus = static_library(
  'xfpmdbus',
  dbus_sources,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('libxfpmdbus'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    libxfce4util,
  ],
  install: false,
)
