if enable_display_settings
  libsettings_common_sources = [
    'debug.c',
    'debug.h',
    'display-name.c',
    'display-profiles.c',
    'display-profiles.h',
    'edid-parse.c',
    'edid.h',
  ]

  if enable_x11
    libsettings_common_sources += [
      'xfce-randr.c',
      'xfce-randr.h',
    ]
  endif

  if enable_wayland
    libsettings_common_sources += [
      'xfce-wlr-output-manager.c',
      'xfce-wlr-output-manager.h',
    ]
  endif

  libsettings_common = static_library(
    'settings-common',
    libsettings_common_sources,
    sources: wayland_protocols_generated_sources,
    c_args: [
      '-DG_LOG_DOMAIN="@0@"'.format('libsettings-common'),
      '-DPNP_IDS="@0@"'.format('pnp_ids_path'),
    ],
    include_directories: [
      include_directories('..'),
    ],
    dependencies: [
      glib,
      gtk,
      libxfce4util,
      xfconf,
      x11_deps,
      xrandr,
      wayland_deps,
      libm,
    ],
    install: false,
  )
endif
