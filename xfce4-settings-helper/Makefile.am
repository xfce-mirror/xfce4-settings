# $Id$

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-helper\" \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	-DAUTOSTART_FILENAME=\"$(autostart_DATA)\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-helper

xfce4_settings_helper_SOURCES = \
	main.c \
	accessibility.c \
	accessibility.h \
	keyboards.c \
	keyboards.h \
	keyboard-shortcuts.c \
	keyboard-shortcuts.h \
	keyboard-layout.c \
	keyboard-layout.h \
	pointers.c \
	pointers.h \
	workspaces.c \
	workspaces.h

xfce4_settings_helper_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(XI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_settings_helper_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_settings_helper_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(LIBWNCK_LIBS)

#
# Optional support for the display settings
#
if HAVE_XRANDR
xfce4_settings_helper_SOURCES += \
	displays.c \
	displays.h

xfce4_settings_helper_CFLAGS += \
	$(XRANDR_CFLAGS)

xfce4_settings_helper_LDADD += \
	$(XRANDR_LIBS)
endif

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = xfce4-settings-helper-autostart.desktop

EXTRA_DIST = \
	xfce4-settings-helper-autostart.desktop

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
