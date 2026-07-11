if enable_x11
  executable(
    'xfce4-mouse-settings',
    [
      'main.c',
      'xfce-device.c',
      'xfce-device.h',
      'xfce-device-manager.c',
      'xfce-device-manager.h',
      'xfce-device-x11.c',
      'xfce-device-x11.h',
      'xfce-device-manager-x11.c',
      'xfce-device-manager-x11.h',
      xfce_revision_h,
      xfce4_settings_resources_c,
    ],
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
      xcursor,
      xorg_libinput,
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
endif
