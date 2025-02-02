editor_sources = [
  'main.c',
  'xfce-settings-cell-renderer.c',
  'xfce-settings-cell-renderer.h',
  'xfce-settings-editor-box.c',
  'xfce-settings-editor-box.h',
  'xfce-settings-prop-dialog.c',
  'xfce-settings-prop-dialog.h',
]

editor_sources += gnome.genmarshal(
  'xfce-settings-marshal',
  sources: 'xfce-settings-marshal.list',
  prefix: '_xfce_settings_marshal',
  internal: true,
  install_header: false,
)

executable(
  'xfce4-settings-editor',
  editor_sources,
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-settings-editor'),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
  input: 'xfce4-settings-editor.desktop.in',
  output: 'xfce4-settings-editor.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)
