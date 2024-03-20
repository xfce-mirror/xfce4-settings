AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DDATADIR=\"$(datadir)\" \
	-DSRCDIR=\"$(top_srcdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-display-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-display-settings

xfce4_display_settings_SOURCES = \
	main.c \
	confirmation-dialog_ui.h \
	display-dialog_ui.h \
	minimal-display-dialog_ui.h \
	profile-changed-dialog_ui.h \
	identity-popup_ui.h \
	scrollarea.c \
	scrollarea.h \
	foo-marshal.c \
	foo-marshal.h \
	display-settings.c \
	display-settings.h

xfce4_display_settings_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_display_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_display_settings_LDADD = \
	$(top_builddir)/common/libxfce4-settings.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS)

if HAVE_XRANDR
xfce4_display_settings_SOURCES += \
	display-settings-x11.c \
	display-settings-x11.h

xfce4_display_settings_CFLAGS += \
	$(XRANDR_CFLAGS) \
	$(LIBX11_CFLAGS)

xfce4_display_settings_LDADD += \
	$(XRANDR_LIBS) \
	$(LIBX11_LIBS)
endif

if ENABLE_WAYLAND
xfce4_display_settings_SOURCES += \
	display-settings-wayland.c \
	display-settings-wayland.h

xfce4_display_settings_CFLAGS += \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(GTK_LAYER_SHELL_CFLAGS)

xfce4_display_settings_LDADD += \
	$(top_builddir)/protocols/libprotocols.la \
	$(WAYLAND_CLIENT_LIBS) \
	$(GTK_LAYER_SHELL_LIBS)
endif

if MAINTAINER_MODE

BUILT_SOURCES = \
	confirmation-dialog_ui.h \
	display-dialog_ui.h \
	minimal-display-dialog_ui.h \
	profile-changed-dialog_ui.h \
	identity-popup_ui.h

confirmation-dialog_ui.h: confirmation-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=confirmation_dialog_ui $< >$@

display-dialog_ui.h: display-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=display_dialog_ui $< >$@

minimal-display-dialog_ui.h: minimal-display-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=minimal_display_dialog_ui $< >$@

profile-changed-dialog_ui.h: profile-changed-dialog.glade
	xdt-csource --static --strip-comments --strip-content --name=profile_changed_dialog_ui $< >$@

identity-popup_ui.h: identity-popup.glade
	xdt-csource --static --strip-comments --strip-content --name=identity_popup_ui $< >$@

endif

desktopdir = $(datadir)/applications
desktop_in_files = xfce-display-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

iconsdir = $(datadir)/icons/hicolor/128x128/devices
icons_DATA = \
	xfce-display-extend-right.png \
	xfce-display-extend-left.png \
	xfce-display-extend-up.png \
	xfce-display-extend-down.png \
	xfce-display-right.png \
	xfce-display-left.png \
	xfce-display-mirror.png \
	xfce-display-profile.png

scalableiconsdir = $(datadir)/icons/hicolor/scalable/devices
scalableicons_DATA = \
	xfce-display-extend-right.svg \
	xfce-display-extend-left.svg \
	xfce-display-extend-up.svg \
	xfce-display-extend-down.svg \
	xfce-display-right.svg \
	xfce-display-left.svg \
	xfce-display-mirror.svg \
	xfce-display-profile.svg

EXTRA_DIST = \
	$(icons_DATA) \
	$(scalableicons_DATA) \
	$(desktop_in_files) \
	confirmation-dialog.glade \
	display-dialog.glade \
	minimal-display-dialog.glade \
	profile-changed-dialog.glade \
	identity-popup.glade

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
