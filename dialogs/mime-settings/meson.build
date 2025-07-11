dialog_sources = [
  'main.c',
  'xfce-mime-chooser.c',
  'xfce-mime-chooser.h',
  'xfce-mime-helper-chooser.c',
  'xfce-mime-helper-chooser.h',
  'xfce-mime-helper.c',
  'xfce-mime-helper.h',
  'xfce-mime-helper-utils.c',
  'xfce-mime-helper-utils.h',
  'xfce-mime-window.c',
  'xfce-mime-window.h',
]

helper_sources = [
  'xfce-mime-helper.c',
  'xfce-mime-helper.h',
  'xfce-mime-helper-chooser.c',
  'xfce-mime-helper-chooser.h',
  'xfce-mime-helper-launcher-dialog.c',
  'xfce-mime-helper-launcher-dialog.h',
  'xfce-mime-helper-main.c',
  'xfce-mime-helper-utils.c',
  'xfce-mime-helper-utils.h',
]

enums = []
enums += gnome.mkenums(
  'xfce-mime-helper-enum-types.h',
  install_header: false,
  sources: 'xfce-mime-helper.h',
  fhead: '#ifndef __XFCE_MIME_HELPER_ENUM_TYPES_H__\n#define __XFCE_MIME_HELPER_ENUM_TYPES_H__\n#include <libxfce4ui/libxfce4ui.h>\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define XFCE_MIME_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n\n#endif /* !__XFCE_MIME_HELPER_ENUM_TYPES_H__ */',
)
enums += gnome.mkenums(
  'xfce-mime-helper-enum-types.c',
  install_header: false,
  sources: 'xfce-mime-helper.h',
  fhead: '#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include "xfce-mime-helper.h"\n\n#include "xfce-mime-helper-enum-types.h"',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n\tstatic GType type = 0;\n\tif (type == 0) {\n\tstatic const G@Type@Value values[] = {',
  vprod: '\t{ @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '\t{ 0, NULL, NULL }\n\t};\n\ttype = g_@type@_register_static ("@EnumName@", values);\n  }\n\treturn type;\n}\n',
)

executable(
  'xfce4-mime-settings',
  dialog_sources + enums,
  sources: xfce_revision_h,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-mime-settings'),
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    gio,
    gio_unix,
    gtk,
    libxfce4ui,
    libxfce4util,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

executable(
  'xfce4-mime-helper',
  helper_sources + enums,
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-mime-settings'),
  ],
  include_directories: [
    include_directories('..' / '..'),
  ],
  dependencies: [
    gio_unix,
    gtk,
    libxfce4ui,
    libxfce4util,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

apps = [
  'xfce4-file-manager.desktop.in',
  'xfce4-mail-reader.desktop.in',
  'xfce4-mime-settings.desktop.in',
  'xfce4-terminal-emulator.desktop.in',
  'xfce4-text-editor.desktop.in',
  'xfce4-web-browser.desktop.in',
]
foreach app : apps
  i18n.merge_file(
    input: app,
    output: fs.stem(app),
    po_dir: '..' / '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
  )
endforeach

helpers = run_command('sh', '-c', 'echo helpers' / '*.desktop.in.in', check: true).stdout().strip().split(' ')
foreach helper_in_in : helpers
  helper_in = configure_file(
    configuration: configuration_data({
      'HELPERDIR': helper_path_prefix / 'xfce4',
    }),
    input: helper_in_in,
    output: fs.stem(helper_in_in),
    install: false,
  )
  i18n.merge_file(
    input: helper_in,
    output: fs.stem(fs.stem(helper_in_in)),
    po_dir: '..' / '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'xfce4' / 'helpers',
  )
endforeach

install_data(
  'helpers' / 'helpers.rc',
  install_dir: get_option('prefix') / get_option('sysconfdir') / 'xdg' / 'xfce4',
)

install_data(
  'helpers' / 'xfce4-compose-mail',
  install_dir: helper_path_prefix / 'xfce4',
  install_mode: 'rwxr-xr-x',
)
