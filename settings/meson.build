settings_sources = [
  '..' / 'common' / 'xfpm-config.h',
  '..' / 'common' / 'xfpm-enum-glib.h',
  '..' / 'common' / 'xfpm-enum.h',
  'xfpm-settings-app.c',
  'xfpm-settings-app.h',
  'xfpm-settings-main.c',
  'xfpm-settings.c',
  'xfpm-settings.h',
]

settings_sources += gnome.gdbus_codegen(
  'xfce-power-manager-dbus',
  sources: '..' / 'src' / 'org.xfce.Power.Manager.xml',
  interface_prefix: 'org.xfce',
  namespace: 'Xfpm',
  install_header: false,
)

executable(
  'xfce4-power-manager-settings',
  settings_sources,
  sources: [
    xfpm_resources_c,
    xfce_revision_h,
  ],
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-power-manager-settings'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gio,
    gio_unix,
    gmodule,
    gthread,
    gtk,
    libxfce4ui,
    libxfce4util,
    upower_glib,
    xfconf,
  ],
  link_with: [
    libxfpmdbus,
    libxfpmcommon,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce4-power-manager-settings.desktop.in',
  output: 'xfce4-power-manager-settings.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)

install_man('xfce4-power-manager-settings.1')
