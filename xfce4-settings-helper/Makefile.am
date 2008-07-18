# $Id$

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-helper\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-helper

xfce4_settings_helper_SOURCES = \
	main.c \
	accessibility.c \
	accessibility.h \
	displays.c \
	displays.h \
	keyboards.c \
	keyboards.h \
	keyboard-shortcuts.c \
	keyboard-shortcuts.h \
	pointers.c \
	pointers.h

xfce4_settings_helper_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(XI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(XRANDR_CFLAGS) \
	$(XXF86VM_CFLAGS)

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
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(XRANDR_LIBS) \
	$(XXF86VM_LIBS)

if HAVE_LIBNOTIFY
xfce4_settings_helper_CFLAGS += \
	$(LIBNOTIFY_CFLAGS)

xfce4_settings_helper_LDADD += \
	$(LIBNOTIFY_LIBS)
endif

if HAVE_HAL
xfce4_settings_helper_CFLAGS += \
	$(HAL_CFLAGS)

xfce4_settings_helper_LDADD += \
	$(HAL_LIBS)
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
