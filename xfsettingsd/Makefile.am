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
	gtk-settings-exported.c \
	gtk-settings-exported.h \
	xsettings-properties.h

xfsettingsd_SOURCES = \
	$(xfsettingsd_built_SOURCES) \
	main.c \
	gtk-decorations.c \
	gtk-decorations.h \
	gtk-settings.c \
	gtk-settings.h

xfsettingsd_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	-DGTK_DISABLE_SINGLE_INCLUDES \
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
	$(GMODULE_LIBS) \
	$(XFCONF_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS)

if ENABLE_X11

xfsettingsd_SOURCES += \
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
	pointers-defines.h \
	workspaces.c \
	workspaces.h \
	xsettings.c \
	xsettings.h

xfsettingsd_CFLAGS += \
	$(LIBXFCE4KBD_PRIVATE_CFLAGS) \
	$(LIBXKLAVIER_CFLAGS) \
	$(XI_CFLAGS) \
	$(LIBX11_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	$(LIBINPUT_CFLAGS)

xfsettingsd_LDADD += \
	$(LIBXFCE4KBD_PRIVATE_LIBS) \
	$(LIBXKLAVIER_LIBS) \
	$(XI_LIBS) \
	$(LIBX11_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(FONTCONFIG_LIBS) \
	$(LIBINPUT_LIBS)

endif # ENABLE_X11

#
# Optional support for the display settings
#
if ENABLE_DISPLAY_SETTINGS

xfsettingsd_SOURCES += \
	displays.c \
	displays.h

xfsettingsd_LDADD += \
	$(top_builddir)/common/libxfce4-settings.la

if HAVE_XRANDR
xfsettingsd_SOURCES += \
	displays-x11.c \
	displays-x11.h

xfsettingsd_CFLAGS += \
	$(XRANDR_CFLAGS)

xfsettingsd_LDADD += \
	$(XRANDR_LIBS)
endif

if ENABLE_WAYLAND
xfsettingsd_SOURCES += \
	displays-wayland.c \
	displays-wayland.h

xfsettingsd_CFLAGS += \
	$(WAYLAND_CLIENT_CFLAGS)

xfsettingsd_LDADD += \
	$(top_builddir)/protocols/libprotocols.la \
	$(WAYLAND_CLIENT_LIBS)
endif

if HAVE_UPOWERGLIB
xfsettingsd_SOURCES += \
	displays-upower.c \
	displays-upower.h

xfsettingsd_CFLAGS += \
	$(UPOWERGLIB_CFLAGS)

xfsettingsd_LDADD += \
	$(UPOWERGLIB_LIBS)
endif

endif # ENABLE_DISPLAY_SETTINGS

moduledir = $(libdir)/gtk-3.0/modules

module_LTLIBRARIES = \
	libxfsettingsd-gtk-settings-sync.la

libxfsettingsd_gtk_settings_sync_la_SOURCES = \
	gtk-settings-module.c

libxfsettingsd_gtk_settings_sync_la_CFLAGS = \
	$(GMODULE_CFLAGS) \
	$(GTK_CFLAGS) \
	$(XFCONF_CFLAGS)

libxfsettingsd_gtk_settings_sync_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^(g_module_check|gtk_module)_init' \
	$(PLATFORM_LDFLAGS)

libxfsettingsd_gtk_settings_sync_la_LIBADD = \
	$(GMODULE_LIBS) \
	$(GTK_LIBS) \
	$(XFCONF_LIBS)

settingsdir = $(sysconfdir)/xdg/xfce4/xfconf/xfce-perchannel-xml
settings_DATA = xsettings.xml

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xfsettingsd.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

# Needed for XfceGtkSettingsHelper to translate GtkSettings property names into
# Xfconf property names
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

# Export on D-Bus by XfceGtkSettingsHelper of the module name xfsettingsd-gtk-settings-sync
# for synchronization of GtkSettings properties with the Xfconf xsettings channel on Wayland
gtk-settings-exported.c:
gtk-settings-exported.h: gtk-settings-exported.xml Makefile
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
	--interface-prefix org.gtk.Settings \
	--c-namespace XfceGtkSettingsHelper \
	--generate-c-code gtk-settings-exported \
	$(srcdir)/gtk-settings-exported.xml

BUILT_SOURCES = \
	$(xfsettingsd_built_SOURCES)

EXTRA_DIST = \
	$(settings_DATA) \
	$(autostart_in_files) \
	gtk-settings-exported.xml

DISTCLEANFILES = \
	$(autostart_DATA) \
	$(xfsettingsd_built_SOURCES)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
