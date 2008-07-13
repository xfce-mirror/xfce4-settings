# $Id$

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-manager\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-manager

xfce4_settings_manager_SOURCES = \
	main.c \
	xfce-settings-manager-dialog.c \
	xfce-settings-manager-dialog.h

xfce4_settings_manager_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(EXO_CFLAGS)

xfce4_settings_manager_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_settings_manager_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(EXO_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce-settings-manager.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
