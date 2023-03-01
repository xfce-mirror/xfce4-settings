AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DSYSCONFIGDIR=\"$(sysconfdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfsettingsd\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfsettingsd

xfsettingsd_built_SOURCES = \
	xsettings-properties.h

xfsettingsd_SOURCES = \
	$(xfsettingsd_built_SOURCES) \
	main.c \
	accessibility.c \
	accessibility.h \
	debug.c \
	debug.h \
	clipboard-manager.c \
	clipboard-manager.h \
	gtk-decorations.c \
	gtk-decorations.h \
	gtk-settings.c \
	gtk-settings.h \
	keyboards.c \
	keyboards.h \
	keyboard-shortcuts.c \
	keyboard-shortcuts.h \
	keyboard-layout.c \
	keyboard-layout.h \
	pointers.c \
	pointers.h \
	pointers-defines.h \
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
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(XI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(LIBINPUT_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
	-DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DG_DISABLE_DEPRECATED

xfsettingsd_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfsettingsd_LDADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(LIBINPUT_LIBS) \
	-lm

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
	$(XRANDR_LIBS) \
	$(top_builddir)/common/libxfce4-settings.la

if HAVE_UPOWERGLIB
xfsettingsd_SOURCES += \
	displays-upower.c \
	displays-upower.h

xfsettingsd_CFLAGS += \
	$(UPOWERGLIB_CFLAGS)

xfsettingsd_LDADD += \
	$(UPOWERGLIB_LIBS)
endif
endif

settingsdir = $(sysconfdir)/xdg/xfce4/xfconf/xfce-perchannel-xml
settings_DATA = xsettings.xml

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xfsettingsd.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

xsettings-properties.h: $(settings_DATA)
	$(AM_V_GEN) \
	printf '%s\n' \
		'#if G_GNUC_CHECK_VERSION (4, 6)' \
		'# pragma GCC diagnostic push' \
		'# pragma GCC diagnostic ignored "-Wunused-variable"' \
		'#endif' > $@; \
	for _prefix in $(shell $(XMLLINT) --xpath '//channel/property/@name' $< | $(SED) -E 's/name="(.*)"/\1/'); do \
		echo "static const gchar *xsettings_properties_$${_prefix}[] = {" \
		&& $(XMLLINT) --xpath "//channel/property[@name=\"$${_prefix}\"]/property/@name" $< | $(SED) -E 's/name=(.*)/\1,/' \
		&& echo '};'; \
	done >> $@; \
	printf '%s\n' \
		'#if G_GNUC_CHECK_VERSION (4, 6)' \
		'# pragma GCC diagnostic pop' \
		'#endif' >> $@;

BUILT_SOURCES = \
	$(xfsettingsd_built_SOURCES)

EXTRA_DIST = \
	$(settings_DATA) \
	$(autostart_in_files)

DISTCLEANFILES = \
	$(autostart_DATA) \
	$(xfsettingsd_built_SOURCES)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
