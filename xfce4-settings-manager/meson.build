menu_dir = get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'menus'
manager_sources = [
  'main.c',
  'xfce-settings-manager-dialog.c',
  'xfce-settings-manager-dialog.h',
]

executable(
  'xfce4-settings-manager',
  manager_sources,
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-settings-manager'),
	  '-DMENUFILE="@0@"'.format(menu_dir / 'xfce-settings-manager.menu')
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gtk,
    exo,
    garcon,
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

install_data('xfce-settings-manager.menu', install_dir: menu_dir)

i18n.merge_file(
  input: 'xfce-settings-manager.desktop.in',
  output: 'xfce-settings-manager.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)
