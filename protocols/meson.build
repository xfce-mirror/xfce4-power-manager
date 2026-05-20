wayland_protocols_generated_sources = []

protocols = {
  'ext-idle-notify-v1': wayland_protocols_pkgdatadir / 'staging' / 'ext-idle-notify' / 'ext-idle-notify-v1.xml',
  'idle-inhibit-unstable-v1': wayland_protocols_pkgdatadir / 'unstable' / 'idle-inhibit' / 'idle-inhibit-unstable-v1.xml',
  'wlr-output-power-management-unstable-v1': wlr_protocols_pkgdatadir / 'unstable' / 'wlr-output-power-management-unstable-v1.xml',
  'wlr-output-management-unstable-v1': wlr_protocols_pkgdatadir / 'unstable' / 'wlr-output-management-unstable-v1.xml',
}

foreach name, file : protocols
  client_header = custom_target(
    '@0@-client-header'.format(name),
    input: file,
    output: '@0@-client.h'.format(name),
    command: [wayland_scanner_bin, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  private_code = custom_target(
    '@0@-private-code'.format(name),
    input: file,
    output: '@0@.c'.format(name),
    command: [wayland_scanner_bin, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  wayland_protocols_generated_sources += [client_header, private_code]
endforeach
