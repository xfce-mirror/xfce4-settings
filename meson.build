project(
  'xfce4-settings',
  'c',
version : '4.21.0',
  license : 'GPL-2.0-or-later',
  meson_version : '>= 0.56.0',
  default_options : ['c_std=gnu11', 'buildtype=debugoptimized', 'warning_level=2']
)

project_namespace = 'xfce'
pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()
copyright_year = '2025'

cc = meson.get_compiler('c')
pkgconfig = import('pkgconfig')
gnome = import('gnome')
i18n = import('i18n')
fs = import('fs')

dependency_versions = {
  'glib': '>= 2.72.0',
  'gtk': '>= 3.24.0',
  'xfce4': '>= 4.18.0',
  'libxfce4ui': '>= 4.21.0',
  'xfconf': '>= 4.19.3',

  'fontconfig': '>= 2.6.0',
  'inputproto': '>= 1.4.0',
  'libx11': '>= 1.6.7',
  'libxext': '>= 1.0.0',
  'libxi': '>= 1.2.0',

  'libxklavier': '>= 5.0',
  'colord': '>= 1.0.2',
  'libnotify': '>= 0.7.8',
  'xcursor': '>= 1.1.0',
  'xorg-libinput': '>= 0.6.0',
  'xrandr': '>= 1.5.0',

  'wayland': '>= 1.20',

  'gtk-layer-shell': '>= 0.7.0',

  'upower-glib': '>= 0.99.10',
}

garcon = dependency('garcon-1', version: dependency_versions['xfce4'])
glib = dependency('glib-2.0', version: dependency_versions['glib'])
gio = dependency('gio-2.0', version: dependency_versions['glib'])
gio_unix = dependency('gio-unix-2.0', version: dependency_versions['glib'])
gmodule = dependency('gmodule-2.0', version: dependency_versions['glib'])
gthread = dependency('gthread-2.0', version: dependency_versions['glib'])
gtk = dependency('gtk+-3.0', version: dependency_versions['gtk'])
libxfce4ui = dependency('libxfce4ui-2', version: dependency_versions['libxfce4ui'])
libxfce4util = dependency('libxfce4util-1.0', version: dependency_versions['xfce4'])
xfconf = dependency('libxfconf-0', version: dependency_versions['xfconf'])

xmllint = find_program('xmllint', required: true)
sed = find_program('sed', required: true)

# Feature: 'x11'
x11_deps = []
x11_deps += dependency('gdk-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('gtk+-x11-3.0', version: dependency_versions['gtk'], required: get_option('x11'))
x11_deps += dependency('fontconfig', version: dependency_versions['fontconfig'], required: get_option('x11'))
x11_deps += dependency('inputproto', version: dependency_versions['inputproto'], required: get_option('x11'))
x11_deps += dependency('libxfce4kbd-private-3', version: dependency_versions['libxfce4ui'], required: get_option('x11'))
x11_deps += dependency('x11', version: dependency_versions['libx11'], required: get_option('x11'))
x11_deps += dependency('xext', version: dependency_versions['libxext'], required: get_option('x11'))
x11_deps += dependency('xi', version: dependency_versions['libxi'], required: get_option('x11'))

# Feature: 'wayland'
wayland_deps = []
wayland_deps += dependency('gdk-wayland-3.0', version: dependency_versions['gtk'], required: get_option('wayland'))
wayland_deps += dependency('wayland-client', version: dependency_versions['wayland'], required: get_option('wayland'))
wayland_deps += dependency('wayland-scanner', version: dependency_versions['wayland'], required: get_option('wayland'), native: true)
wayland_scanner = wayland_deps[-1]

feature_cflags = []

enable_x11 = not get_option('x11').disabled()
foreach dep : x11_deps
  enable_x11 = enable_x11 and dep.found()
endforeach
if enable_x11
  feature_cflags += '-DENABLE_X11=1'

  libnotify = dependency('libnotify', version: dependency_versions['libnotify'], required: get_option('libnotify'))
  if libnotify.found()
    feature_cflags += '-DHAVE_LIBNOTIFY=1'
  endif
  libxklavier = dependency('libxklavier', version: dependency_versions['libxklavier'], required: get_option('libxklavier'))
  if libxklavier.found()
    feature_cflags += '-DHAVE_LIBXKLAVIER=1'
  endif
  xcursor = dependency('xcursor', version: dependency_versions['xcursor'], required: get_option('xcursor'))
  if xcursor.found()
    feature_cflags += '-DHAVE_XCURSOR=1'
  endif
  xorg_libinput = dependency('xorg-libinput', version: dependency_versions['xorg-libinput'], required: get_option('xorg-libinput'))
  if xorg_libinput.found()
    feature_cflags += '-DHAVE_LIBINPUT=1'
  endif
  xrandr = dependency('xrandr', version: dependency_versions['xrandr'], required: get_option('xrandr'))
  if xrandr.found()
    feature_cflags += '-DHAVE_XRANDR=1'
  endif
else
  libnotify = dependency('', required: false)
  libxklavier = dependency('', required: false)
  xcursor = dependency('', required: false)
  xorg_libinput = dependency('', required: false)
  xrandr = dependency('', required: false)
endif

enable_wayland = not get_option('wayland').disabled()
foreach dep : wayland_deps
  enable_wayland = enable_wayland and dep.found()
endforeach
if enable_wayland
  feature_cflags += '-DENABLE_WAYLAND=1'

  wayland_scanner_path = wayland_scanner.get_variable(pkgconfig: 'wayland_scanner')
  wayland_scanner_bin = find_program(wayland_scanner_path, required: true)

  wlr_protocols = dependency('wlr-protocols', required: false)
  if wlr_protocols.found()
    wlr_protocols_pkgdatadir = wlr_protocols.get_variable(pkgconfig: 'pkgdatadir')
  else
    wlr_protocols_pkgdatadir = meson.project_source_root() / 'protocols' / 'wlr-protocols'
    if not fs.is_dir(wlr_protocols_pkgdatadir / 'unstable')
      error(wlr_protocols_pkgdatadir + ' seems empty, did you clone the wlr-protocols submodule?')
    endif
  endif

  gtk_layer_shell = dependency('gtk-layer-shell-0', version: dependency_versions['gtk-layer-shell'], required: get_option('gtk-layer-shell'))
  if gtk_layer_shell.found()
    feature_cflags += '-DHAVE_GTK_LAYER_SHELL=1'
  endif
else
  gtk_layer_shell = dependency('', required: false)
endif

if not enable_x11 and not enable_wayland
  error('At least one of the X11 and Wayland backends must be enabled')
endif

if enable_wayland or xrandr.found()
  enable_display_settings = true
  feature_cflags += '-DENABLE_DISPLAY_SETTINGS=1'

  upower_glib = dependency('upower-glib', version: dependency_versions['upower-glib'], required: get_option('upower'))
  if upower_glib.found()
    feature_cflags += '-DHAVE_UPOWERGLIB=1'
  endif
else
  enable_display_settings = false
  upower_glib = dependency('', required: false)
endif

colord = dependency('colord', version: dependency_versions['colord'], required: get_option('colord'))

pnp_ids_path = get_option('pnp-ids-path')
if pnp_ids_path == ''
  pnp_ids_path = get_option('prefix') / get_option('datadir') / 'hwdata' / 'pnp.ids'
endif

helper_path_prefix = get_option('helper-path-prefix')
if helper_path_prefix == ''
  helper_path_prefix = get_option('prefix') / get_option('libdir')
endif

if get_option('sound-settings')
  feature_cflags += '-DENABLE_SOUND_SETTINGS=1'
endif

functions = [
  'daemon',
  'setsid',
]
foreach function : functions
  if cc.has_function(function)
    feature_cflags += '-DHAVE_@0@=1'.format(function.to_upper())
  endif
endforeach

headers = [
  'math.h',
  'sys/wait.h',
]
foreach header : headers
  if cc.check_header(header)
    feature_cflags += '-DHAVE_@0@=1'.format(header.underscorify().to_upper())
  endif
endforeach

libm = cc.find_library('m', required: true)

extra_cflags = []
extra_cflags_check = [
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wold-style-definition',
  '-Wredundant-decls',
  '-Wpointer-arith',
  '-Wcast-align',
  '-Winit-self',
  '-Wshadow',
  '-Wmissing-include-dirs',
  '-Wundef',
  '-Wformat',
  '-Wformat-security',
  '-Wformat-y2k',
  '-Wnested-externs',
  '-Wno-unused-parameter',
  '-Wno-declaration-after-statement',
  '-Wno-missing-field-initializers',
  '-Werror=implicit-function-declaration',
  '-Wno-error=deprecated-declarations',
]

optimization = get_option('optimization')
if get_option('debug') and optimization in ['0', 'g']
  extra_cflags_check += '-fstack-protector-strong'
  extra_cflags += [
    '-DDEBUG=1',
    '-DDEBUG_TRACE=1',
    '-DG_ENABLE_DEBUG',
  ]
elif optimization in ['3', 'minsize']
  extra_cflags += [
    '-DNDEBUG',
    '-DG_DISABLE_CAST_CHECKS',
    '-DG_DISABLE_ASSERT',
  ]
endif

if dependency_versions.has_key('glib')
  glib_version_parts = dependency_versions['glib'].split(' ')
  glib_min_version_parts = glib_version_parts[1].split('.')
  glib_min_version_define = 'GLIB_VERSION_@0@_@1@'.format(glib_min_version_parts[0], glib_min_version_parts[1])
  extra_cflags += [
    '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_min_version_define),
    '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_min_version_define),
    '-DG_LOG_USE_STRUCTURED=1',
  ]
endif

version_parts = meson.project_version().split('-dev')[0].split('.')
version_short = '@0@.@1@'.format(version_parts[0], version_parts[1])

extra_cflags += [
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DVERSION_SHORT="@0@"'.format(version_short),
  '-DPACKAGE_STRING="@0@ @1@"'.format(meson.project_name(), meson.project_version()),
  '-DPACKAGE_DATADIR="@0@"'.format(pkgdatadir),
  '-DCOPYRIGHT_YEAR="@0@"'.format(copyright_year),
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPACKAGE_BUGREPORT="https://gitlab.xfce.org/@0@/@1@/-/issues"'.format(project_namespace, meson.project_name()),
  '-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name()),
  '-DPREFIX="@0@"'.format(get_option('prefix')),
  '-DBINDIR="@0@"'.format(get_option('prefix') / get_option('bindir')),
  '-DDATADIR="@0@"'.format(get_option('prefix') / get_option('datadir')),
  '-DINCLUDEDIR="@0@"'.format(get_option('prefix') / get_option('includedir')),
  '-DLIBDIR="@0@"'.format(get_option('prefix') / get_option('libdir')),
  '-DLIBEXECDIR="@0@"'.format(get_option('prefix') / get_option('libexecdir')),
  '-DLOCALEDIR="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DLOCALSTATEDIR="@0@"'.format(get_option('prefix') / get_option('localstatedir')),
  '-DSBINDIR="@0@"'.format(get_option('prefix') / get_option('sbindir')),
  '-DSYSCONFDIR="@0@"'.format(get_option('prefix') / get_option('sysconfdir')),
  '-DHAVE_XFCE_REVISION_H=1',
]

add_project_arguments(cc.get_supported_arguments(extra_cflags_check), language: 'c')
add_project_arguments(extra_cflags, language: 'c')
add_project_arguments(feature_cflags, language: 'c')

xfce_revision_h = vcs_tag(
  command: ['git', 'rev-parse', '--short', 'HEAD'],
  fallback: 'UNKNOWN',
  input: 'xfce-revision.h.in',
  output: 'xfce-revision.h',
  replace_string: '@REVISION@',
)

subdir('protocols')
subdir('common')
subdir('dialogs')
subdir('dialogs' / 'display-settings')
subdir('dialogs' / 'mime-settings')
subdir('icons')
subdir('po')
subdir('xfce4-settings-editor')
subdir('xfce4-settings-manager')
subdir('xfsettingsd')
