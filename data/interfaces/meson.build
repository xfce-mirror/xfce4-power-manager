xfpm_settings_ui_h = custom_target(
  'xfpm-settings_ui.h',
  input: 'xfpm-settings.ui',
  output: 'xfpm-settings_ui.h',
  command: [xdt_csource, '--static', '--strip-comments', '--strip-content', '--name=xfpm_settings_ui', '--output=@OUTPUT@', '@INPUT@'],
)
