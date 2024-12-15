dnl
dnl Copyright (c) 2006 - 2024
dnl         The Xfce development team. All rights reserved.
dnl
dnl Originally written for Xfce by Benedikt Meurer <benny@xfce.org>
dnl Modified by Stephan Arts <stephan@xfce.org>
dnl

dnl ***************************
dnl *** Version information ***
dnl ***************************
XDT_VERSION_INIT([4.20.0])

dnl minimum required versions
m4_define([glib_min_version], [2.72.0])
m4_define([gtk_min_version], [3.24.0])
m4_define([garcon_min_version], [0.1.10])
m4_define([exo_min_version], [0.11.0])
m4_define([libxfce4util_min_version], [4.17.2])
m4_define([libxfce4ui_min_version], [4.19.5])
m4_define([xfconf_min_version], [4.13.0])

m4_define([libx11_min_version], [1.6.7])
m4_define([fontconfig_min_version], [2.6.0])
m4_define([xi_min_version], [1.2.0])
m4_define([inputproto_min_version], [1.4.0])

m4_define([xrandr_min_version], [1.5.0])
m4_define([libnotify_min_version], [0.7.8])
m4_define([colord_min_version], [1.0.2])
m4_define([xcursor_min_version], [1.1.0])
m4_define([xorg_libinput_min_version], [0.6.0])
m4_define([libxklavier_min_version], [0.3.0])
m4_define([libxklavier4_min_version], [4.0])
m4_define([libxklavier5_min_version], [5.0])

m4_define([wayland_min_version], [1.20])

m4_define([gtk_layer_shell_min_version], [0.7.0])

m4_define([upower_glib_min_version], [0.99.10])

dnl define a short version string for xfce4-settings
m4_append([xfce4_settings_version_short], ["])
m4_append([xfce4_settings_version_short], [xdt_version_major])
m4_append([xfce4_settings_version_short], [.])
m4_append([xfce4_settings_version_short], [xdt_version_minor])
m4_append([xfce4_settings_version_short], ["])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2008-2024
        The Xfce development team. All rights reserved.])
AC_INIT([xfce4-settings], [xdt_version], [https://gitlab.xfce.org/xfce/xfce4-settings])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])
AC_REVISION([xdt_version_build])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.11 dist-bzip2 tar-ustar no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl *******************************
dnl *** Check for UNIX variants ***
dnl *******************************
AC_USE_SYSTEM_EXTENSIONS
AC_SEARCH_LIBS([strerror],[cposix])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AM_PROG_CC_C_O
LT_PATH_LD([])
AC_PROG_INSTALL()
AC_PROG_SED
AC_ARG_VAR([XMLLINT], [xmllint])
AC_PATH_PROG([XMLLINT], [xmllint])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([Could not find xmllint in PATH])
fi
XDT_CHECK_PACKAGE_BINARY([GDBUS_CODEGEN], [gio-2.0], [gdbus_codegen], [gdbus-codegen])

dnl **************************
dnl *** Initialize libtool ***
dnl **************************
LT_PREREQ([2.4])
LT_INIT([disable-static])

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_CHECK_HEADERS([errno.h memory.h math.h stdlib.h string.h unistd.h signal.h time.h sys/types.h sys/wait.h])
AC_CHECK_FUNCS([daemon setsid])
AC_SEARCH_LIBS([round], [m], [], [AC_MSG_ERROR([unable to find the round() function in math library])])

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl ***********************************
dnl *** Version string for the docs ***
dnl ***********************************
AC_DEFINE([XFCE4_SETTINGS_VERSION_SHORT], [xfce4_settings_version_short], [Short version string of xfce4-settings])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_min_version])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GARCON], [garcon-1], [garcon_min_version])
XDT_CHECK_PACKAGE([EXO], [exo-2], [exo_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [libxfce4util_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [libxfce4ui_min_version])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [xfconf_min_version])

dnl ***********************************
dnl *** Check for optional packages ***
dnl ***********************************
XDT_CHECK_OPTIONAL_FEATURE([X11],
                           [x11],
                           [
                             XDT_FEATURE_DEPENDENCY([LIBX11], [x11], [libx11_min_version])
                             XDT_FEATURE_DEPENDENCY([GDK_X11], [gdk-x11-3.0], [gtk_min_version])
                             XDT_FEATURE_DEPENDENCY([GTK_X11], [gtk+-x11-3.0], [gtk_min_version])
                             XDT_FEATURE_DEPENDENCY([LIBXFCE4KBD_PRIVATE], [libxfce4kbd-private-3], [libxfce4ui_min_version])
                             XDT_FEATURE_DEPENDENCY([FONTCONFIG], [fontconfig], [fontconfig_min_version])
                             XDT_FEATURE_DEPENDENCY([XI], [xi], [xi_min_version])
                             XDT_FEATURE_DEPENDENCY([INPUTPROTO], [inputproto], [inputproto_min_version])
                           ],
                           [the X11 windowing system])
XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
                           [wayland],
                           [
                             XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [gtk_min_version])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_CLIENT], [wayland-client], [wayland_min_version])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_SCANNER], [wayland-scanner], [wayland_min_version])
                           ],
                           [the Wayland windowing system])
if test x"$ENABLE_X11" != x"yes" -a x"$ENABLE_WAYLAND" != x"yes"; then
  AC_MSG_ERROR([Either both X11 and Wayland support was disabled, or required dependencies are missing. One of the two must be enabled.])
fi

if test x"$ENABLE_X11" != x"yes"; then
  dnl Do not check for optional X11 packages if X11 is disabled
  enable_xrandr=no
  enable_xcursor=no
  enable_xorg_libinput=no
  enable_libxklavier=no
  enable_libnotify=no
fi
XDT_CHECK_OPTIONAL_PACKAGE([XRANDR], [xrandr], [xrandr_min_version], [xrandr], [Xrandr support])
XDT_CHECK_OPTIONAL_PACKAGE([XCURSOR], [xcursor], [xcursor_min_version], [xcursor], [Cursor themes support])
XDT_CHECK_OPTIONAL_PACKAGE([LIBINPUT], [xorg-libinput], [xorg_libinput_min_version], [xorg-libinput], [libinput Xorg driver support])
XDT_CHECK_OPTIONAL_PACKAGE([LIBXKLAVIER], [libxklavier], [libxklavier_min_version], [libxklavier], [Keyboard layout selection])
XDT_CHECK_OPTIONAL_PACKAGE([LIBXKLAVIER4], [libxklavier], [libxklavier4_min_version], [libxklavier], [Keyboard layout selection])
XDT_CHECK_OPTIONAL_PACKAGE([LIBXKLAVIER5], [libxklavier], [libxklavier5_min_version], [libxklavier], [Keyboard layout selection])
XDT_CHECK_OPTIONAL_PACKAGE([LIBNOTIFY], [libnotify], [libnotify_min_version], [libnotify], [Notification support])

if test x"$ENABLE_WAYLAND" = x"yes"; then
  WLR_PROTOCOLS_PKGDATADIR=`$PKG_CONFIG --variable=pkgdatadir wlr-protocols 2>/dev/null || true`
  if test x"$WLR_PROTOCOLS_PKGDATADIR" = x""; then
    WLR_PROTOCOLS_PKGDATADIR="$ac_abs_confdir/protocols/wlr-protocols"
    if test "$(echo "$WLR_PROTOCOLS_PKGDATADIR/"*)" = "$WLR_PROTOCOLS_PKGDATADIR/*"; then
      AC_MSG_ERROR([$WLR_PROTOCOLS_PKGDATADIR seems empty, did you clone the wlr-protocols submodule?])
    fi
  fi
  AC_MSG_CHECKING([for wlr-protocols])
  AC_MSG_RESULT([$WLR_PROTOCOLS_PKGDATADIR])
  AC_SUBST([WLR_PROTOCOLS_PKGDATADIR])
else
  dnl Do not check for optional Wayland packages if Wayland is disabled
  enable_gtk_layer_shell=no
fi
XDT_CHECK_OPTIONAL_PACKAGE([GTK_LAYER_SHELL], [gtk-layer-shell-0], [gtk_layer_shell_min_version], [gtk-layer-shell], [GTK Layer Shell support])

if test x"$XRANDR_FOUND" != x"yes" -a x"$ENABLE_WAYLAND" != x"yes"; then
  dnl Do not check for upower-glib if xrandr is not found and Wayland is disabled
  enable_upower_glib=no
else
  ENABLE_DISPLAY_SETTINGS="yes"
  AC_DEFINE([ENABLE_DISPLAY_SETTINGS], [1], [Enable display seetings])
fi
AM_CONDITIONAL([ENABLE_DISPLAY_SETTINGS], [test x"$XRANDR_FOUND" = x"yes" -o x"$ENABLE_WAYLAND" = x"yes"])
XDT_CHECK_OPTIONAL_PACKAGE([UPOWERGLIB], [upower-glib], [upower_glib_min_version], [upower-glib], [UPower support], [no])

dnl ***********************************
dnl *** Optional support for hwdata ***
dnl ***********************************
AC_ARG_WITH([pnp-ids-path],
            [AS_HELP_STRING([--with-pnp-ids-path],[Specify the path to pnp.ids (default=\${datadir}/hwdata/pnp.ids)])],
            [with_pnp_ids_path=$withval],
            [with_pnp_ids_path="\${datadir}/hwdata/pnp.ids"])
PNP_IDS=$with_pnp_ids_path
AC_SUBST(PNP_IDS)

dnl **************************************
dnl *** Optional support for colord ***
dnl **************************************
XDT_CHECK_OPTIONAL_PACKAGE([COLORD], [colord], [colord_min_version],
                           [colord], [colord support])

dnl *********************
dnl *** Mime settings ***
dnl *********************
XDT_CHECK_OPTIONAL_PACKAGE([GIO_UNIX], [gio-unix-2.0], [glib_min_version],
                           [gio-unix], [Mime settings])

dnl ****************************************
dnl *** Optional support for Libcanberra ***
dnl ****************************************

dnl Camberra XSettings
AC_ARG_ENABLE([sound-settings],
              [AS_HELP_STRING([--enable-sound-settings],[Enable sound XSETTINGS in GUI (from libcanberra) (default=no)])],
              [enable_sound_settings=$enableval],
              [enable_sound_settings=no])
if test x"$enable_sound_settings" = x"yes"; then
  AC_DEFINE([ENABLE_SOUND_SETTINGS], [1], [Define to enable sound control in UI])
  ENABLE_SOUND_SETTINGS=1
fi
AC_SUBST(ENABLE_SOUND_SETTINGS)

dnl ******************************
dnl *** Set helper path prefix ***
dnl ******************************
AC_ARG_WITH([helper-path-prefix],
            [AS_HELP_STRING([--with-helper-path-prefix=PATH],[Path prefix under which helper executables will be installed (default: $libdir)])],
            [HELPER_PATH_PREFIX="$withval"],
            [HELPER_PATH_PREFIX="$libdir"])
AC_SUBST([HELPER_PATH_PREFIX])

dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])

dnl **************************************
dnl *** Check for linker optimizations ***
dnl **************************************
XDT_FEATURE_LINKER_OPTS()

dnl *********************************
dnl *** Substitute platform flags ***
dnl *********************************
AC_MSG_CHECKING([PLATFORM_CPPFLAGS])
AC_MSG_RESULT([$PLATFORM_CPPFLAGS])
AC_SUBST([PLATFORM_CPPFLAGS])
AC_MSG_CHECKING([PLATFORM_CFLAGS])
AC_MSG_RESULT([$PLATFORM_CFLAGS])
AC_SUBST([PLATFORM_CFLAGS])
AC_MSG_CHECKING([PLATFORM_LDFLAGS])
AC_MSG_RESULT([$PLATFORM_LDFLAGS])
AC_SUBST([PLATFORM_LDFLAGS])

AC_CONFIG_FILES([
Makefile
po/Makefile.in
protocols/Makefile
common/Makefile
dialogs/Makefile
dialogs/appearance-settings/Makefile
dialogs/accessibility-settings/Makefile
dialogs/color-settings/Makefile
dialogs/display-settings/Makefile
dialogs/keyboard-settings/Makefile
dialogs/mime-settings/Makefile
dialogs/mime-settings/helpers/Makefile
dialogs/mouse-settings/Makefile
icons/Makefile
icons/16x16/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/48x48/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
xfsettingsd/Makefile
xfce4-settings-manager/Makefile
xfce4-settings-editor/Makefile
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
echo
echo "* Installation prefix:              $prefix"
echo "* Debug Support:                    $enable_debug"
echo "* X11 Support:                      ${ENABLE_X11:-no}"
if test x"$ENABLE_X11" = x"yes"; then
echo "  * Xrandr support:                 ${XRANDR_FOUND:-no}"
echo "  * Xcursor support:                ${XCURSOR_FOUND:-no}"
echo "  * Xorg libinput support:          ${LIBINPUT_FOUND:-no}"
echo "  * Libxklavier support:            ${LIBXKLAVIER_FOUND:-no}"
echo "  * Libnotify support:              ${LIBNOTIFY_FOUND:-no}"
fi
echo "* Wayland Support:                  ${ENABLE_WAYLAND:-no}"
if test x"$ENABLE_WAYLAND" = x"yes"; then
echo "  * GTK Layer Shell support:        ${GTK_LAYER_SHELL_FOUND:-no}"
fi
if test x"$ENABLE_DISPLAY_SETTINGS" = x"yes"; then
echo "* UPower support:                   ${UPOWERGLIB_FOUND:-no}"
fi
echo "* Colord support:                   ${COLORD_FOUND:-no}"
echo "* Mime settings (gio-unix) support: ${GIO_UNIX_FOUND:-no}"
echo "* Sounds settings support:          $enable_sound_settings"
echo
