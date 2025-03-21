AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DHELPERDIR=\"$(HELPER_PATH_PREFIX)/xfce4/settings\" \
	-DG_LOG_DOMAIN=\"xfce4-appearance-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-appearance-settings

xfce4_appearance_settings_SOURCES = \
	$(top_srcdir)/common/resources/xfce4-settings-resources.c \
	main.c \
	$(NULL)

xfce4_appearance_settings_CFLAGS = \
	$(GTHREAD_CFLAGS) \
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

xfce4_appearance_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_appearance_settings_LDADD = \
	$(GTHREAD_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS)

xfce4_appearance_scriptsdir = $(HELPER_PATH_PREFIX)/xfce4/settings
xfce4_appearance_scripts_SCRIPTS = appearance-install-theme

desktopdir = $(datadir)/applications
desktop_in_files = xfce-ui-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(desktop_in_files) \
	appearance-install-theme \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
