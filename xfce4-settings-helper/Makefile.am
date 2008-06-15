bin_PROGRAMS = xfce4-settings-helper

xfce4_settings_helper_SOURCES = \
	main.c \
	accessx.c accessx.h

xfce4_settings_helper_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfce4_settings_helper_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS)

INCLUDES = \
	-I$(top_srcdir)
