plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

shared_module(
  'xfce4powermanager',
  [
    'power-manager-button.c',
    'power-manager-button.h',
    'power-manager-config.c',
    'power-manager-config.h',
    'power-manager-dialog.c',
    'power-manager-dialog.h',
    'power-manager-plugin.c',
    'power-manager-plugin.h',
    'scalemenuitem.c',
    'scalemenuitem.h',
  ],
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-power-manager-plugin'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
    upower_glib,
    xfconf,
  ],
  link_with: [
    libxfpmcommon,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: 'power-manager-plugin.desktop.in',
  output: 'power-manager-plugin.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
