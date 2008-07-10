bin_PROGRAMS = xfce4-appearance-settings 

xfce4_appearance_settings_SOURCES = \
	main.c \
	images.h \
	appearance-dialog_glade.h

xfce4_appearance_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GLADE_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(XFCONF_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfce4_appearance_settings_LDADD = \
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
appearance-dialog_glade.h

appearance-dialog_glade.h: appearance-dialog.glade
	exo-csource --static --name=appearance_dialog_glade $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-appearance-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

settingsdir = $(datadir)/xfce4/settings-dialogs
settings_DATA = $(desktop_DATA)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)							\
	appearance-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)
