SUBDIRS =								\
	helpers

AM_CPPFLAGS = \
	-I${top_srcdir} \
	-DBINDIR=\"$(bindir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-mime-settings\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-mime-settings

xfce4_mime_helper_built_sources =						\
	xfce-mime-helper-enum-types.c						\
	xfce-mime-helper-enum-types.h

xfce_mime_helper_headers =							\
	xfce-mime-helper.h

xfce4_mime_settings_SOURCES = \
	$(xfce4_mime_helper_built_sources)					\
	$(xfce_mime_helper_headers)						\
	main.c \
	xfce-mime-helper-utils.c						\
	xfce-mime-helper-utils.h						\
	xfce-mime-helper.c							\
	xfce-mime-helper-chooser.c	\
	xfce-mime-helper-chooser.h	\
	xfce-mime-chooser.c \
	xfce-mime-chooser.h \
	xfce-mime-window.c \
	xfce-mime-window.h

xfce4_mime_settings_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GIO_UNIX_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(EXO_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS) \
    -DGTK_DISABLE_SINGLE_INCLUDES \
    -DGDK_DISABLE_DEPRECATED \
    -DGTK_DISABLE_DEPRECATED \
    -DGSEAL_ENABLE \
    -DGDK_PIXBUF_DISABLE_DEPRECATED \
    -DG_DISABLE_DEPRECATED

xfce4_mime_settings_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_mime_settings_LDADD = \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GIO_UNIX_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS)	\
	$(EXO_LIBS) \
	$(XFCONF_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-mime-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)
