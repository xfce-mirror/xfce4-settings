# $Id$

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfsettingsd\" \
	-DAUTOSTART_FILENAME=\"$(autostart_DATA)\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfsettingsd

xfsettingsd_SOURCES = \
	main.c \
	accessibility.c \
	accessibility.h \
	clipboard-manager.c \
	clipboard-manager.h \
	keyboards.c \
	keyboards.h \
	keyboard-shortcuts.c \
	keyboard-shortcuts.h \
	keyboard-layout.c \
	keyboard-layout.h \
	pointers.c \
	pointers.h \
	workspaces.c \
	workspaces.h \
	xsettings.c \
	xsettings.h

xfsettingsd_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(XI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfsettingsd_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfsettingsd_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(FONTCONFIG_LIBS)

#
# Optional support for the display settings
#
if HAVE_XRANDR
xfsettingsd_SOURCES += \
	displays.c \
	displays.h

xfsettingsd_CFLAGS += \
	$(XRANDR_CFLAGS)

xfsettingsd_LDADD += \
	$(XRANDR_LIBS)
endif

settingsdir = $(sysconfdir)/xdg/xfce4/xfconf/xfce-perchannel-xml
settings_DATA = xsettings.xml

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = xfsettingsd-autostart.desktop

EXTRA_DIST = \
	$(settings_DATA) \
	$(autostart_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
