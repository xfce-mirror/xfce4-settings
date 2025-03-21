if enable_display_settings
  dialog_sources = [
    'display-settings.c',
    'display-settings.h',
    'foo-marshal.c',
    'foo-marshal.h',
    'main.c',
    'scrollarea.c',
    'scrollarea.h',
  ]

  if xrandr.found()
    dialog_sources += [
      'display-settings-x11.c',
      'display-settings-x11.h',
    ]
  endif

  if enable_wayland
    dialog_sources += [
      'display-settings-wayland.c',
      'display-settings-wayland.h',
    ]
  endif

  executable(
    'xfce4-display-settings',
    dialog_sources,
    sources: [
      xfce_revision_h,
      xfce4_settings_resources_c,
    ],
    c_args: [
      '-DG_LOG_DOMAIN="@0@"'.format('xfce4-display-settings'),
    ],
    include_directories: [
      include_directories('..' / '..'),
    ],
    dependencies: [
      glib,
      gtk,
      libxfce4ui,
      xfconf,
      x11_deps,
      xrandr,
      wayland_deps,
      gtk_layer_shell,
      libm,
    ],
    link_with: [
      libsettings_common,
    ],
    install: true,
    install_dir: get_option('prefix') / get_option('bindir'),
  )

  i18n.merge_file(
    input: 'xfce-display-settings.desktop.in',
    output: 'xfce-display-settings.desktop',
    po_dir: '..' / '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
  )

  icon_names = [
    'xfce-display-extend-down',
    'xfce-display-extend-left',
    'xfce-display-extend-right',
    'xfce-display-extend-up',
    'xfce-display-left',
    'xfce-display-mirror',
    'xfce-display-profile',
    'xfce-display-right',
  ]
  foreach icon_name : icon_names
    install_data(
      '@0@.png'.format(icon_name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'devices'
    )
    install_data(
      '@0@.svg'.format(icon_name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'devices'
    )
  endforeach
endif
