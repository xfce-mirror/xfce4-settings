executable(
  'xfce4-accessibility-settings',
  [
    'main.c',
    xfce_revision_h,
    xfce4_settings_resources_c,
  ],
  c_args: [
    '-DG_LOG_DOMAIN="xfce4-accessibility-settings"',
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
    x11_deps,
    xrandr,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

find_cursor_sources = [
  'find-cursor.c',
]
if enable_wayland
  find_cursor_sources += wayland_protocols_generated_sources
endif

executable(
  'xfce4-find-cursor',
  find_cursor_sources,
  c_args: [
    '-DG_LOG_DOMAIN="xfce4-find-cursor"',
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    gtk,
    xfconf,
    wayland_deps,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce4-accessibility-settings.desktop.in',
  output: 'xfce4-accessibility-settings.desktop',
  po_dir: '..' / '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)
