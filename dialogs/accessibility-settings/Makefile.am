bin_PROGRAMS = xfce4-accessibility-settings 

xfce4_accessibility_settings_SOURCES = \
	main.c \
	accessibility-dialog_glade.h

xfce4_accessibility_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GLADE_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(XFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfce4_accessibility_settings_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GLADE_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(XFCONF_LIBS)

INCLUDES = \
	-I${top_srcdir}

if MAINTAINER_MODE

BUILT_SOURCES = \
accessibility-dialog_glade.h

accessibility-dialog_glade.h: accessibility-dialog.glade
	exo-csource --static --name=accessibility_dialog_glade $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-accessibility-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

settingsdir = $(datadir)/xfce4/settings-dialogs
settings_DATA = $(desktop_DATA)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)							\
	accessibility-dialog.glade

