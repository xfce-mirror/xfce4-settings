bin_PROGRAMS = xfce4-settings-manager

INCLUDES = \
	-DLOCALEDIR=\"$(localedir)\" \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(EXO_CFLAGS)

LIBS = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(EXO_LIBS)

xfce4_settings_manager_SOURCES = \
	main.c \
	xfce-settings-manager-dialog.c \
	xfce-settings-manager-dialog.h

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-settings-manager.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)
