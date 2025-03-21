AM_CPPFLAGS = \
	-I${top_srcdir}	\
	-DPNP_IDS=\"$(PNP_IDS)\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libxfce4-settings.la

libxfce4_settings_built_sources = \
	xfce4-settings-resources.c \
	$(NULL)

libxfce4_settings_la_SOURCES = \
	$(libxfce4_settings_built_sources) \
	debug.c \
	debug.h \
	$(NULL)

libxfce4_settings_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

libxfce4_settings_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libxfce4_settings_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(XFCONF_LIBS)

if ENABLE_DISPLAY_SETTINGS

libxfce4_settings_la_SOURCES += \
	display-name.c \
	display-profiles.c \
	display-profiles.h \
	edid-parse.c \
	edid.h

if HAVE_XRANDR
libxfce4_settings_la_SOURCES += \
	xfce-randr.c \
	xfce-randr.h

libxfce4_settings_la_CFLAGS += \
	$(XRANDR_CFLAGS) \
	$(LIBX11_CFLAGS)

libxfce4_settings_la_LIBADD += \
	$(XRANDR_LIBS) \
	$(LIBX11_LIBS)
endif

if ENABLE_WAYLAND
libxfce4_settings_la_SOURCES += \
	xfce-wlr-output-manager.c \
	xfce-wlr-output-manager.h

libxfce4_settings_la_CFLAGS += \
	$(WAYLAND_CLIENT_CFLAGS)

libxfce4_settings_la_LIBADD += \
	$(top_builddir)/protocols/libprotocols.la \
	$(WAYLAND_CLIENT_LIBS)
endif

endif # ENABLE_DISPLAY_SETTINGS

libxfce4_settings_resources_deps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir)/resources $(srcdir)/resources/xfce4-settings.gresource.xml)

xfce4-settings-resources.c: $(srcdir)/resources/xfce4-settings.gresource.xml $(libxfce4_settings_resources_deps) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir)/resources --generate-source $<

BUILT_SOURCES = \
	$(libxfce4_settings_built_sources) \
	$(NULL)

DISTCLEANFILES = \
	$(libxfce4_settings_built_sources) \
	$(NULL)

resources = \
	resources/accessibility-dialog.glade \
	resources/appearance-dialog.glade \
	resources/color-dialog.glade \
	resources/confirmation-dialog.glade \
	resources/display-dialog.glade \
	resources/identity-popup.glade \
	resources/keyboard-dialog.glade \
	resources/minimal-display-dialog.glade \
	resources/mouse-dialog.glade \
	resources/profile-changed-dialog.glade \
	resources/xfce4-settings.gresource.xml \
	$(NULL)

EXTRA_DIST = \
	$(resources) \
	$(NULL)
