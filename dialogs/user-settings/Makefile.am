
INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-user-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-user-settings

xfce4_user_settings_SOURCES = \
	avatar_default.h \
	main.c \
	user-dialog_ui.h

xfce4_user_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(EXO_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(ACCOUNTSSERVICE_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_user_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_user_settings_LDADD = \
	$(GTK_LIBS) \
	$(EXO_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(ACCOUNTSSERVICE_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	user-dialog_ui.h \
	avatar_default.h

user-dialog_ui.h: user-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=user_dialog_ui $< >$@

avatar_default.h: $(srcdir)/avatar-default.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --stream --name=avatar_default $< > $@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-user-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	avatar-default.png \
	user-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
