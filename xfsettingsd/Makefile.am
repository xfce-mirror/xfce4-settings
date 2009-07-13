# $Id$

if BUILD_XFSETTINGSD

INCLUDES = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfsettingsd\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfsettingsd

xfsettingsd_SOURCES = \
	main.c \
	registry.c \
	registry.h

xfsettingsd_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfsettingsd_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfsettingsd_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS)

settingsdir = $(sysconfdir)/xdg/xfce4/xfconf/xfce-perchannel-xml
settings_DATA = xsettings.xml

EXTRA_DIST = \
	$(settings_DATA)

endif
