sizes = [16, 24, 32, 48, 128]
names = [
  'org.xfce.filemanager',
  'org.xfce.mailreader',
  'org.xfce.settings.accessibility',
  'org.xfce.settings.appearance',
  'org.xfce.settings.color',
  'org.xfce.settings.default-applications',
  'org.xfce.settings.display',
  'org.xfce.settings.editor',
  'org.xfce.settings.keyboard',
  'org.xfce.settings.manager',
  'org.xfce.settings.mouse',
  'org.xfce.terminalemulator',
  'org.xfce.webbrowser',
]

foreach size : sizes
  foreach name : names
    install_data(
      '@0@x@0@'.format(size) / '@0@.png'.format(name),
      install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
    )
  endforeach
endforeach

foreach name : names
  install_data(
    'scalable' / '@0@.svg'.format(name),
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  )
endforeach
