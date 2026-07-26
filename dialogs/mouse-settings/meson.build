mouse_settings_sources = [
  'main.c',
  'xfce-device.c',
  'xfce-device.h',
  'xfce-device-manager.c',
  'xfce-device-manager.h',
  xfce_revision_h,
  xfce4_settings_resources_c,
]

if enable_x11
  mouse_settings_sources += [
    'xfce-device-x11.c',
    'xfce-device-x11.h',
    'xfce-device-manager-x11.c',
    'xfce-device-manager-x11.h',
  ]
endif

if enable_wayland
  mouse_settings_sources += [
    'xfce-device-wayland.c',
    'xfce-device-wayland.h',
    'xfce-device-manager-wayland.c',
    'xfce-device-manager-wayland.h',
  ]
endif

executable(
  'xfce4-mouse-settings',
  mouse_settings_sources,
  c_args: [
    '-DG_LOG_DOMAIN="xfce4-mouse-settings"',
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
    wayland_deps,
    xcursor,
    libm,
  ],
  link_with: [libsettings_common],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce4-mouse-settings.desktop.in',
  output: 'xfce4-mouse-settings.desktop',
  po_dir: '..' / '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)
