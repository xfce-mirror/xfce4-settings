dirs = [
  'appearance-settings',
]

if enable_x11
  dirs += [
    'accessibility-settings',
    'keyboard-settings',
    'mouse-settings',
  ]
endif

if colord.found()
  dirs += [
    'color-settings',
  ]
endif

foreach dir : dirs
  name = dir.split('-')[0]
  if name == 'accessibility'
    sources = [dir / 'main.c']
  else
    sources = run_command('sh', '-c', 'echo @0@'.format(dir) / '*.[ch]', check: true).stdout().strip().split(' ')
  endif

  ui_h = '@0@-dialog_ui.h'.format(name)
  ui_xml = '@0@-dialog.glade'.format(name)
  ui_id = fs.stem(ui_h).underscorify()
  sources += custom_target(
    ui_h,
    input: dir / ui_xml,
    output: ui_h,
    command: [xdt_csource, '--static', '--strip-comments', '--strip-content', '--name=' + ui_id, '--output=@OUTPUT@', '@INPUT@'],
  )

  bin = 'xfce4-@0@'.format(dir)
  executable(
    bin,
    sources,
    sources: xfce_revision_h,
    c_args: [
      '-DG_LOG_DOMAIN="@0@"'.format('bin'),
      '-DHELPERDIR="@0@"'.format(helper_path_prefix),
    ],
    include_directories: [
      include_directories('..'),
    ],
    dependencies: [
      gtk,
      libxfce4ui,
      xfconf,
      x11_deps,
      colord,
      libxklavier,
      xcursor,
      xorg_libinput,
      libm,
    ],
    install: true,
    install_dir: get_option('prefix') / get_option('bindir'),
  )

  desktop = run_command('sh', '-c', 'echo @0@'.format(dir) / '*.desktop.in', check: true).stdout().strip()
  i18n.merge_file(
    input: desktop,
    output: fs.stem(desktop),
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
  )

  if name == 'accessibility'
    executable(
      'xfce4-find-cursor',
      [dir / 'find-cursor.c'],
      c_args: [
        '-DG_LOG_DOMAIN="@0@"'.format('bin'),
      ],
      include_directories: [
        include_directories('..'),
      ],
      dependencies: [
        gtk,
        xfconf,
      ],
      install: true,
      install_dir: get_option('prefix') / get_option('bindir'),
    )
  endif

  if name == 'appearance'
    install_data(
      dir / 'appearance-install-theme',
      install_dir: helper_path_prefix / 'xfce4' / 'settings',
      install_mode: 'rwxr-xr-x',
    )
  endif
endforeach
