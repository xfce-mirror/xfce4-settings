# $Id$

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-display-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-display-settings

xfce4_display_settings_SOURCES = \
	main.c \
	xfce-randr.c \
	xfce-randr.h \
	xfce-randr-legacy.c \
	xfce-randr-legacy.h \
	display-dialog_glade.h

xfce4_display_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_display_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_display_settings_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(XRANDR_LIBS)

if MAINTAINER_MODE

BUILT_SOURCES = \
	display-dialog_glade.h

display-dialog_glade.h: display-dialog.glade
	exo-csource --static --strip-comments --strip-content --name=display_dialog_glade $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-display-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files) \
	display-dialog.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
