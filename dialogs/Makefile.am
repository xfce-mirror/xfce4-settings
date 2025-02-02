SUBDIRS = \
	appearance-settings \
	mime-settings

if ENABLE_X11
SUBDIRS += \
	accessibility-settings \
	keyboard-settings \
	mouse-settings
endif

if ENABLE_DISPLAY_SETTINGS
SUBDIRS += \
	display-settings
endif

if HAVE_COLORD
SUBDIRS += \
	color-settings
endif

EXTRA_DIST = \
	meson.build \
	$(NULL)
