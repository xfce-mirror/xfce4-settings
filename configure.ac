dnl
dnl Copyright (c) 2006 - 2023
dnl         The Xfce development team. All rights reserved.
dnl
dnl Originally written for Xfce by Benedikt Meurer <benny@xfce.org>
dnl Modified by Stephan Arts <stephan@xfce.org>
dnl

dnl ***************************
dnl *** Version information ***
dnl ***************************
XDT_VERSION_INIT([4.18.0], [git])

dnl minimum required versions
m4_define([glib_min_version], [2.66.0])
m4_define([gtk_min_version], [3.24.0])
m4_define([x11_minimum_version], [1.6.7])
m4_define([garcon_min_version], [0.1.10])
m4_define([exo_min_version], [0.11.0])
m4_define([libxfce4util_min_version], [4.17.2])
m4_define([libxfce4ui_min_version], [4.17.0])
m4_define([xfconf_min_version], [4.13.0])

m4_define([fontconfig_min_version], [2.6.0])
m4_define([xi_min_version], [1.2.0])
m4_define([inputproto_min_version], [1.4.0])

m4_define([xrandr_min_version], [1.2.0])
m4_define([upower_glib_min_version], [0.9.8])
m4_define([libnotify_min_version], [0.1.3])
m4_define([colord_min_version], [1.0.2])
m4_define([xcursor_min_version], [1.1.0])
m4_define([xorg_libinput_min_version], [0.6.0])
m4_define([libxklavier_min_version], [0.3.0])
m4_define([libxklavier4_min_version], [4.0])
m4_define([libxklavier5_min_version], [5.0])

dnl define a short version string for xfce4-settings
m4_append([xfce4_settings_version_short], ["])
m4_append([xfce4_settings_version_short], [xdt_version_major])
m4_append([xfce4_settings_version_short], [.])
m4_append([xfce4_settings_version_short], [xdt_version_minor])
m4_append([xfce4_settings_version_short], ["])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2008-2023
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
AC_PATH_PROG([XMLLINT], [xmllint], [xmllint])
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
XDT_CHECK_PACKAGE([LIBX11], [x11], [x11_min_version])
XDT_CHECK_PACKAGE([GARCON], [garcon-1], [garcon_min_version])
XDT_CHECK_PACKAGE([EXO], [exo-2], [exo_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [libxfce4util_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [libxfce4ui_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4KBD_PRIVATE], [libxfce4kbd-private-3], [libxfce4ui_min_version])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [xfconf_min_version])

XDT_CHECK_PACKAGE([FONTCONFIG], [fontconfig], [fontconfig_min_version])

XDT_CHECK_PACKAGE([XI], [xi], [xi_min_version], [],
[
  for dir in /usr/X11R6 /usr/X11 /opt/X11R6 /opt/X11; do
    test -z "$XI_CFLAGS" || break
    saved_CFLAGS="$CFLAGS"
    saved_LIBS="$LIBS"
    CFLAGS="$CFLAGS -I$dir/include"
    LIBS="$LIBS -L$dir/lib"
    AC_CHECK_LIB([Xi], [XListInputDevices],
    [
      XI_CFLAGS="-I$dir/include"
      XI_LIBS="-L$dir/lib -lXi"
      AC_SUBST(XI_CFLAGS)
      AC_SUBST(XI_LIBS)
    ])
    CFLAGS="$saved_CFLAGS"
    LIBS="$saved_LIBS"
  done
  if test -z "$XI_CFLAGS"; then
    echo "*** The required package Xi was not found on your system."
    echo "*** Please install Xi or adjust the PKG_CONFIG_PATH environment"
    echo "*** variable if you installed the package in a nonstandard"
    echo "*** prefix so that pkg-config is able to find it."
    exit 1
  fi
])

XDT_CHECK_PACKAGE([INPUTPROTO], [inputproto], [inputproto_min_version])

dnl ***********************************
dnl *** Optional support for Xrandr ***
dnl ***********************************
XDT_CHECK_OPTIONAL_PACKAGE([XRANDR], [xrandr], [xrandr_min_version],
                           [xrandr], [Xrandr support])

dnl ***********************************
dnl *** Optional support for hwdata ***
dnl ***********************************
AC_ARG_WITH([pnp-ids-path],
            [AS_HELP_STRING([--with-pnp-ids-path],[Specify the path to pnp.ids (default=\${datadir}/hwdata/pnp.ids)])],
            [with_pnp_ids_path=$withval],
            [with_pnp_ids_path="\${datadir}/hwdata/pnp.ids"])
PNP_IDS=$with_pnp_ids_path
AC_SUBST(PNP_IDS)

dnl ***********************************
dnl *** Optional support for UPower ***
dnl ***********************************
XDT_CHECK_OPTIONAL_PACKAGE([UPOWERGLIB], [upower-glib], [upower_glib_min_version],
                           [upower-glib], [UPower support], [no])

dnl **************************************
dnl *** Optional support for Libnotify ***
dnl **************************************
XDT_CHECK_OPTIONAL_PACKAGE([LIBNOTIFY], [libnotify], [libnotify_min_version],
                           [libnotify], [Notification support])

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

dnl ************************************
dnl *** Optional support for Xcursor ***
dnl ************************************
XDT_CHECK_OPTIONAL_PACKAGE([XCURSOR], [xcursor], [xcursor_min_version],
                           [xcursor], [Cursor themes support])

dnl *************************************************
dnl *** Optional support for libinput Xorg driver ***
dnl *************************************************
XDT_CHECK_OPTIONAL_PACKAGE([LIBINPUT], [xorg-libinput], [xorg_libinput_min_version],
                           [xorg-libinput], [libinput Xorg driver support])

dnl ****************************************
dnl *** Optional support for Libxklavier ***
dnl ****************************************
XDT_CHECK_OPTIONAL_PACKAGE([LIBXKLAVIER], [libxklavier], [libxklavier_min_version],
                           [libxklavier], [Keyboard layout selection])
XDT_CHECK_OPTIONAL_PACKAGE([LIBXKLAVIER4], [libxklavier], [libxklavier4_min_version],
                           [libxklavier], [Keyboard layout selection])
XDT_CHECK_OPTIONAL_PACKAGE([LIBXKLAVIER5], [libxklavier], [libxklavier5_min_version],
                           [libxklavier], [Keyboard layout selection])

dnl make pluggable settings dialogs optional
AC_ARG_ENABLE([pluggable-dialogs],
              [AS_HELP_STRING([--enable-pluggable-dialogs],[Enable support for embedded settings dialogs (default=disabled)])],
              [enable_pluggable_dialogs=$enableval],
              [enable_pluggable_dialogs=no])
if test x"$enable_pluggable_dialogs" = x"yes"; then
  AC_DEFINE([ENABLE_PLUGGABLE_DIALOGS], [1], [Define to enable embedded settings dialogs])
  ENABLE_PLUGGABLE_DIALOGS=1
fi
AC_SUBST(ENABLE_PLUGGABLE_DIALOGS)

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
echo "* Installation prefix:       $prefix"
echo "* Debug Support:             $enable_debug"
if test x"$XRANDR_FOUND" = x"yes"; then
echo "* Xrandr support:            yes"
else
echo "* Xrandr support:            no"
fi
if test x"$UPOWERGLIB_FOUND" = x"yes"; then
echo "* UPower support:            yes"
else
echo "* UPower support:            no"
fi
if test x"$LIBNOTIFY_FOUND" = x"yes"; then
echo "* Libnotify support:         yes"
else
echo "* Libnotify support:         no"
fi
if test x"$COLORD_FOUND" = x"yes"; then
echo "* colord support:            yes"
else
echo "* colord support:            no"
fi
if test x"$XCURSOR_FOUND" = x"yes"; then
echo "* Xcursor support:           yes"
else
echo "* Xcursor support:           no"
fi
if test x"$LIBINPUT_FOUND" = x"yes"; then
echo "* Xorg libinput support:     yes"
else
echo "* Xorg libinput support:     no"
fi
if test x"$ENABLE_PLUGGABLE_DIALOGS" = x"1"; then
echo "* Embedded settings dialogs  yes"
else
echo "* Embedded settings dialogs  no"
fi
if test x"$ENABLE_SOUND_SETTINGS" = x"1"; then
echo "* Sounds settings support    yes"
else
echo "* Sounds settings support    no"
fi
if test x"$LIBXKLAVIER_FOUND" = x"yes"; then
echo "* Libxklavier support:       yes"
else
echo "* Libxklavier support:       no"
fi
if test x"$GIO_UNIX_FOUND" = x"yes"; then
echo "* Mime settings (gio-unix):  yes"
else
echo "* Mime settings (gio-unix):  no"
fi
echo