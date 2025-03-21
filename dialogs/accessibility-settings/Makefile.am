AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-accessibility-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-accessibility-settings \
	xfce4-find-cursor

xfce4_find_cursor_SOURCES = \
  find-cursor.c

xfce4_find_cursor_CFLAGS = \
	$(GTK_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

xfce4_find_cursor_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_find_cursor_LDADD = \
	$(GTK_LIBS) \
	$(XFCONF_LIBS)

xfce4_accessibility_settings_SOURCES = \
	main.c \
	$(NULL)

xfce4_accessibility_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

xfce4_accessibility_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_accessibility_settings_LDADD = \
	$(top_builddir)/common/libxfce4-settings.la \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-accessibility-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
