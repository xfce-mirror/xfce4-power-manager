i18n.merge_file(
  input: 'appdata' / 'xfce4-power-manager.appdata.xml.in',
  output: 'xfce4-power-manager.appdata.xml',
  po_dir: '..' / 'po',
  type: 'xml',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'metainfo',
)

app_sizes = [16, 24, 32, 48, 64, 96, 128]
app_names = [
  'org.xfce.powermanager',
]
status_sizes = [32, 48]
status_names = [
  'ac-adapter',
  'battery-caution-charging',
  'battery-caution',
  'battery-empty-charging',
  'battery-empty',
  'battery-full-charged',
  'battery-full-charging',
  'battery-full',
  'battery-good-charging',
  'battery-good',
  'battery-low-charging',
  'battery-low',
  'battery-missing',
  'computer',
  'display-brightness',
  'input-keyboard',
  'input-mouse',
  'keyboard-brightness',
  'multimedia-player',
  'pda',
  'phone',
  'tablet',
  'uninterruptible-power-supply',
  'video-display',
]

foreach size : app_sizes
  foreach name : app_names
    install_data(
      'icons' / '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
    )
  endforeach
endforeach

foreach size : status_sizes
  foreach name : status_names
    install_data(
      'icons' / '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'status',
    )
  endforeach
endforeach

foreach name : app_names
  install_data(
    'icons' / 'scalable' / '@0@.svg'.format(name),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  )
endforeach

svgs = run_command('sh', '-c', 'echo ' +  'icons' / 'scalable' / '*.svg', check: true).stdout().strip().split(' ')

# see https://gitlab.xfce.org/xfce/xfce4-power-manager/-/issues/268
if meson.version().version_compare('>= 1.3.0')
  foreach svg : svgs
    if fs.stem(svg) not in app_names
      install_data(
        svg,
        install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'status',
        follow_symlinks: true,
      )
    endif
  endforeach
else
  foreach svg : svgs
    if fs.stem(svg) not in app_names
      install_data(
        svg,
        install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'status',
      )
    endif
  endforeach
endif
