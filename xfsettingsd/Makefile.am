if BUILD_XFSETTINGSD

bin_PROGRAMS = xfsettingsd

xfsettingsd_SOURCES = \
	main.c \
	registry.c registry.h

xfsettingsd_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\"

xfsettingsd_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS)

INCLUDES = \
	-I$(top_srcdir)

endif
