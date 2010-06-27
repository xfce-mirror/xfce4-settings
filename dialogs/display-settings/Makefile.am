# $Id$

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-display-settings\" \
	-DPNP_IDS=\"$(PNP_IDS)\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-display-settings

xfce4_display_settings_SOURCES = \
	main.c \
	xfce-randr.c \
	xfce-randr.h \
	xfce-randr-legacy.c \
	xfce-randr-legacy.h \
	confirmation-dialog_ui.h \
	display-dialog_ui.h \
	minimal-display-dialog_ui.h \
	display-name.c \
	edid-parse.c \
	edid.h

xfce4_display_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_display_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_display_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(EXO_LIBS) \
	$(XRANDR_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	confirmation-dialog_ui.h
	display-dialog_ui.h \
	minimal-display-dialog_ui.h

confirmation-dialog_ui.h: confirmation-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=confirmation_dialog_ui $< >$@

display-dialog_ui.h: display-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=display_dialog_ui $< >$@

minimal-display-dialog_ui.h: minimal-display-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=minimal_display_dialog_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-display-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	confirmation-dialog.glade \
	display-dialog.glade \
	minimal-display-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
