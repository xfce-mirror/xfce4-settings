xfsettingsd_sources = [
  'main.c',
  'gtk-decorations.c',
  'gtk-decorations.h',
  'gtk-settings.c',
  'gtk-settings.h',
]

xfsettingsd_sources += gnome.gdbus_codegen(
  'gtk-settings-exported',
  sources: 'gtk-settings-exported.xml',
  interface_prefix: 'org.gtk.Settings',
  namespace: 'XfceGtkSettingsHelper',
  install_header: false,
)

xsettings_xml = 'xsettings.xml'
contents = [
  '#if G_GNUC_CHECK_VERSION (4, 6)',
  '# pragma GCC diagnostic push',
  '# pragma GCC diagnostic ignored "-Wunused-variable"',
  '#endif',
]

# Use env vars XPATH and SED_SCRIPT to avoid any issue with backslash escaping, see
# https://github.com/mesonbuild/meson/issues/1564
# It also makes things a bit more readable
command = [
  'sh',
  '-c',
  xmllint.full_path()
    + ' --xpath "$XPATH" "$MESON_SOURCE_ROOT"' / 'xfsettingsd' / xsettings_xml
    + ' | '
    + sed.full_path()
    + ' -E "$SED_SCRIPT"',
]

prefixes = run_command(
  command,
  env: {
    'XPATH': '//channel/property/@name',
    'SED_SCRIPT': 's/name="(.*)"/\\1/',
  },
  check: true
).stdout().strip().split('\n')

foreach prefix : prefixes
  prefix = prefix.strip()

  properties = run_command(
    command,
    env: {
      'XPATH': '//channel/property[@name="@0@"]/property/@name'.format(prefix),
      'SED_SCRIPT': 's/name=(.*)/\\1,/',
    },
    check: true
  ).stdout().strip().split('\n')

  contents += [
    'static const gchar *xsettings_properties_' + prefix + '[] = {',
    properties,
    '};',
  ]
endforeach

contents += [
  '#if G_GNUC_CHECK_VERSION (4, 6)',
  '# pragma GCC diagnostic pop',
  '#endif',
]

xfsettingsd_sources += custom_target(
  'xsettings-properties.h',
  output: 'xsettings-properties.h',
  command: ['sh', '-c', 'printf "%s\n" "$@"', '_', contents],
  capture: true,
)

if enable_x11
  xfsettingsd_sources += [
    'accessibility.c',
    'accessibility.h',
    'keyboards.c',
    'keyboards.h',
    'keyboard-shortcuts.c',
    'keyboard-shortcuts.h',
    'keyboard-layout.c',
    'keyboard-layout.h',
    'pointers.c',
    'pointers.h',
    'pointers-defines.h',
    'workspaces.c',
    'workspaces.h',
    'xsettings.c',
    'xsettings.h',
  ]
endif

link_with = []
if enable_display_settings
  link_with += libsettings_common

  xfsettingsd_sources += [
    'displays.c',
    'displays.h',
  ]

  if xrandr.found()
    xfsettingsd_sources += [
      'displays-x11.c',
      'displays-x11.h',
    ]
  endif

  if enable_wayland
    xfsettingsd_sources += [
      'displays-wayland.c',
      'displays-wayland.h',
    ]
  endif

  if upower_glib.found()
    xfsettingsd_sources += [
      'displays-upower.c',
      'displays-upower.h',
    ]
  endif
endif

executable(
  'xfsettingsd',
  xfsettingsd_sources,
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfsettingsd'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gio,
    gmodule,
    gthread,
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
    x11_deps,
    libnotify,
    libxklavier,
    xorg_libinput,
    xrandr,
    wayland_deps,
    upower_glib,
    libm,
  ],
  link_with: link_with,
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

shared_module(
  'xfsettingsd-gtk-settings-sync',
  [
    'gtk-settings-module.c',
  ],
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfsettingsd'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gmodule,
    gtk,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'gtk-3.0' / 'modules'
)

install_data(
  xsettings_xml,
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'xfce4' / 'xfconf' / 'xfce-perchannel-xml'
)

i18n.merge_file(
  input: 'xfsettingsd.desktop.in',
  output: 'xfsettingsd.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'autostart',
)
