AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-color-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-color-settings

xfce4_color_settings_SOURCES = \
	$(top_srcdir)/common/xfce4-settings-resources.c \
	main.c \
	color-device.c \
	color-device.h \
	color-profile.c \
	color-profile.h \
	$(NULL)

xfce4_color_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(COLORD_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

xfce4_color_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_color_settings_LDADD = \
	$(GTK_LIBS) \
	$(COLORD_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(NULL)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-color-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
