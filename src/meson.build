xfpm_sources = [
  'xfpm-backlight.c',
  'xfpm-backlight.h',
  'xfpm-battery.c',
  'xfpm-battery.h',
  'xfpm-button.c',
  'xfpm-button.h',
  'xfpm-dpms.c',
  'xfpm-dpms.h',
  'xfpm-errors.c',
  'xfpm-errors.h',
  'xfpm-idle.c',
  'xfpm-idle.h',
  'xfpm-inhibit.c',
  'xfpm-inhibit.h',
  'xfpm-kbd-backlight.c',
  'xfpm-kbd-backlight.h',
  'xfpm-main.c',
  'xfpm-manager.c',
  'xfpm-manager.h',
  'xfpm-notify.c',
  'xfpm-notify.h',
  'xfpm-polkit.c',
  'xfpm-polkit.h',
  'xfpm-power.c',
  'xfpm-power.h',
  'xfpm-ppd.h',
  'xfpm-ppd.c',
  'xfpm-suspend.c',
  'xfpm-suspend.h',
  'xfpm-xfconf.c',
  'xfpm-xfconf.h',
]

if enable_x11
  xfpm_sources += [
    'xfpm-dpms-x11.c',
    'xfpm-dpms-x11.h',
    'xfpm-idle-x11.c',
    'xfpm-idle-x11.h',
  ]
endif

if enable_wayland
  xfpm_sources += wayland_protocols_generated_sources
  xfpm_sources += [
    'xfpm-dpms-wayland.c',
    'xfpm-dpms-wayland.h',
    'xfpm-idle-wayland.c',
    'xfpm-idle-wayland.h',
  ]
endif

xfpm_sources += gnome.genmarshal(
  'xfpm-marshal',
  sources: 'xfpm-marshal.list',
  prefix: '_xfpm_marshal',
  internal: true,
  install_header: false,
)

xfpm_sources += gnome.gdbus_codegen(
  'xfce-power-manager-dbus',
  sources: 'org.xfce.Power.Manager.xml',
  interface_prefix: 'org.xfce',
  namespace: 'Xfpm',
  install_header: false,
)
xfpm_sources += gnome.gdbus_codegen(
  'org.freedesktop.PowerManagement',
  sources: 'org.freedesktop.PowerManagement.xml',
  interface_prefix: 'org.freedesktop',
  namespace: 'Xfpm',
  install_header: false,
)
xfpm_sources += gnome.gdbus_codegen(
  'org.freedesktop.PowerManagement.Inhibit',
  sources: 'org.freedesktop.PowerManagement.Inhibit.xml',
  interface_prefix: 'org.freedesktop',
  namespace: 'Xfpm',
  install_header: false,
)

executable(
  'xfce4-power-manager',
  xfpm_sources,
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-power-manager'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gio,
    gio_unix,
    gobject,
    gthread,
    gtk,
    libnotify,
    libxfce4ui,
    libxfce4util,
    upower_glib,
    xfconf,
    x11_deps,
    wayland_deps,
  ],
  link_with: [
    libxfpmdbus,
    libxfpmcommon,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce4-power-manager.desktop.in',
  output: 'xfce4-power-manager.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'autostart',
)

install_man('xfce4-power-manager.1')

if polkit.found()
  executable(
    'xfpm-power-backlight-helper',
    [
      'xfpm-backlight-helper.c',
    ],
    c_args: [
      '-DG_LOG_DOMAIN="@0@"'.format('xfce4-power-manager'),
    ],
    include_directories: [
      include_directories('..'),
    ],
    dependencies: [
      glib,
      libxfce4util,
    ],
    install: true,
    install_dir: get_option('prefix') / get_option('sbindir'),
  )

  executable(
    'xfce4-pm-helper',
    [
      'xfpm-pm-helper.c',
    ],
    c_args: [
      '-DG_LOG_DOMAIN="@0@"'.format('xfce4-power-manager'),
    ],
    include_directories: [
      include_directories('..'),
    ],
    dependencies: [
      glib,
      libxfce4util,
    ],
    install: true,
    install_dir: get_option('prefix') / get_option('sbindir'),
  )

  policy_in = configure_file(
    configuration: configuration_data({
      'sbindir': get_option('prefix') / get_option('sbindir'),
    }),
    input: 'org.xfce.power.policy.in.in',
    output: 'org.xfce.power.policy.in',
    install: false,
  )

  i18n.merge_file(
    input: policy_in,
    output: 'org.xfce.power.policy',
    po_dir: '..' / 'po',
    type: 'xml',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'polkit-1' / 'actions',
  )
endif
