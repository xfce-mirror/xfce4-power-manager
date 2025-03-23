project(
  'xfce4-power-manager',
  'c',
version : '4.21.0',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.56.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'xfce'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2025'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.72.0',
  'gtk': '>= 3.24.0',
  'libnotify': '>= 0.7.8',
  'xfce4': '>= 4.18.0',
  'libxfce4ui': '>= 4.21.0',
  'libxfce4util': '>= 4.19.4',
  'upower-glib': '>= 0.99.10',

  'libx11': '>= 1.6.7',
  'libxext': '>= 1.0.0',
  'xrandr': '>= 1.5.0',

  'wayland': '>= 1.20',
  'wayland-protocols': '>= 1.25',

  'polkit': '>= 0.102',
}

glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gio_unix = dependency('gio-unix-2.0', version: dependency_versions['glib'])
gmodule = dependency('gmodule-2.0', version: dependency_versions['glib'])
gobject = dependency('gobject-2.0', version: dependency_versions['glib'])
gthread = dependency('gthread-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libnotify = dependency('libnotify', version: dependency_versions['libnotify'])
upower_glib = dependency('upower-glib', version: dependency_versions['upower-glib'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['libxfce4util'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfce4'])

# Feature: 'x11'
x11_deps = []
x11_deps += dependency('gdk-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('x11', version: dependency_versions['libx11'], required: get_option('x11'))
x11_deps += dependency('xext', version: dependency_versions['libxext'], required: get_option('x11'))
x11_deps += dependency('xrandr', version: dependency_versions['xrandr'], required: get_option('x11'))

# Feature: 'wayland'
wayland_deps = []
wayland_deps += dependency('gdk-wayland-3.0', version: dependency_versions['gtk'], required: get_option('wayland'))
wayland_deps += dependency('wayland-client', version: dependency_versions['wayland'], required: get_option('wayland'))
wayland_deps += dependency('wayland-scanner', version: dependency_versions['wayland'], required: get_option('wayland'), native: true)
wayland_scanner = wayland_deps[-1]
wayland_deps += dependency('wayland-protocols', version: dependency_versions['wayland-protocols'], required: get_option('wayland'))
wayland_protocols = wayland_deps[-1]

feature_cflags = []

enable_x11 = not get_option('x11').disabled()
foreach dep : x11_deps
  enable_x11 = enable_x11 and dep.found()
endforeach
if enable_x11
  feature_cflags += '-DENABLE_X11=1'

  if cc.has_header_symbol('X11/XF86keysym.h', 'XF86XK_Suspend')
    feature_cflags += '-DHAVE_XF86XK_SUSPEND=1'
  endif
  if cc.has_header_symbol('X11/XF86keysym.h', 'XF86XK_Hibernate')
    feature_cflags += '-DHAVE_XF86XK_HIBERNATE=1'
  endif
endif

enable_wayland = not get_option('wayland').disabled()
foreach dep : wayland_deps
  enable_wayland = enable_wayland and dep.found()
endforeach
if enable_wayland
  feature_cflags += '-DENABLE_WAYLAND=1'

  wayland_scanner_path = wayland_scanner.get_variable(pkgconfig: 'wayland_scanner')
  wayland_scanner_bin = find_program(wayland_scanner_path, required: true)
  wayland_protocols_pkgdatadir = wayland_protocols.get_variable(pkgconfig: 'pkgdatadir')

  wlr_protocols = dependency('wlr-protocols', required: false)
  if wlr_protocols.found()
    wlr_protocols_pkgdatadir = wlr_protocols.get_variable(pkgconfig: 'pkgdatadir')
  else
    wlr_protocols_pkgdatadir = meson.project_source_root() / 'protocols' / 'wlr-protocols'
    if not fs.is_dir(wlr_protocols_pkgdatadir / 'unstable')
      error(wlr_protocols_pkgdatadir + ' seems empty, did you clone the wlr-protocols submodule?')
    endif
  endif

  # FIXME: Bump wayland-protocols to 1.27 when it is an acceptable requirement,
  # and remove this and protocols/ext-idle-notify-v1.xml
  if fs.is_file(wayland_protocols_pkgdatadir / 'staging' / 'ext-idle-notify' / 'ext-idle-notify-v1.xml')
    ext_idle_notify_xml = wayland_protocols_pkgdatadir / 'staging' / 'ext-idle-notify' / 'ext-idle-notify-v1.xml'
  else
    ext_idle_notify_xml = 'ext-idle-notify-v1.xml'
  endif
endif

if not enable_x11 and not enable_wayland
  error('At least one of the X11 and Wayland backends must be enabled')
endif

libxfce4panel = dependency('libxfce4panel-2.0', version: dependency_versions['xfce4'], required: get_option('panel-plugin'))
polkit = dependency('polkit-gobject-1', version: dependency_versions['polkit'], required: get_option('polkit'))
if polkit.found()
  feature_cflags += '-DHAVE_POLKIT=1'
endif

host_os = get_option('host-os')
if host_os == ''
  host_os = host_machine.system()
endif
if host_os == 'linux'
  feature_cflags += '-DBACKEND_TYPE_LINUX=1'
elif host_os in ['freebsd', 'dragonfly']
  feature_cflags += '-DBACKEND_TYPE_FREEBSD=1'
elif host_os == 'openbsd'
  feature_cflags += '-DBACKEND_TYPE_OPENBSD=1'
endif

functions = [
  'setsid',
  'sigprocmask',
]
foreach function : functions
  if cc.has_function(function)
    feature_cflags += '-DHAVE_@0@=1'.format(function.to_upper())
  endif
endforeach

headers = [
  'sys/param.h',
  'sys/resource.h',
  'sys/stat.h',
  'sys/sysctl.h',
  'sys/types.h',
  'sys/user.h',
  'sys/wait.h',
  'errno.h',
  'math.h',
  'memory.h',
  'stdlib.h',
  'signal.h',
  'string.h',
  'unistd.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

libm = cc.find_library('m', required: true)

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

if enable_wayland
  subdir('protocols')
endif
subdir('common')
subdir('data')
subdir('data' / 'interfaces')
subdir('libdbus')
if libxfce4panel.found()
  subdir('panel-plugin')
endif
subdir('po')
subdir('settings')
subdir('src')
