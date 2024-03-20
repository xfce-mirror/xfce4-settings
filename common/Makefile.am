if ENABLE_DISPLAY_SETTINGS

AM_CPPFLAGS = \
	-I${top_srcdir}	\
	-DPNP_IDS=\"$(PNP_IDS)\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libxfce4-settings.la

libxfce4_settings_la_SOURCES = \
	debug.c \
	debug.h \
	display-name.c \
	display-profiles.c \
	display-profiles.h \
	edid-parse.c \
	edid.h

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
