bin_PROGRAMS = xfce4-settings-manager

INCLUDES = \
	-DLOCALEDIR=\"$(localedir)\" \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS)

LIBS = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCEGUI4_LIBS)

xfce4_settings_manager_SOURCES = \
	main.c \
	xfce-settings-manager-dialog.c \
	xfce-settings-manager-dialog.h
