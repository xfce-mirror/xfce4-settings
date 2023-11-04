SUBDIRS = \
	appearance-settings

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

if HAVE_GIO_UNIX
SUBDIRS += \
	mime-settings
endif

if HAVE_COLORD
SUBDIRS += \
	color-settings
endif
