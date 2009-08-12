# $Id: Makefile.am 27279 2008-07-13 19:24:20Z nick $

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-editor\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-editor

xfce4_settings_editor_SOURCES = \
	main.c \
	main_window.c main_window.h \
	xfce4-settings-editor_ui.h

xfce4_settings_editor_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(EXO_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_settings_editor_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_settings_editor_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(EXO_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-settings-editor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	xfce4-settings-editor.ui

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE

BUILT_SOURCES = \
	xfce4-settings-editor_ui.h

xfce4-settings-editor_ui.h: xfce4-settings-editor.ui
	exo-csource --static --strip-comments --strip-content --name=xfce4_settings_editor_ui $< >$@

endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
