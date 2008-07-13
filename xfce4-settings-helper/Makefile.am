# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DBINDIR=\"$(bindir)\"						\
	-DDATADIR=\"$(datadir)\"					\
	-DG_LOG_DOMAIN=\"Xfce4-settings-helper\"			\
	-DLIBDIR=\"$(libdir)\"						\
	-DLIBEXECDIR=\"$(libexecdir)\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS =								\
	xfce4-settings-helper

xfce4_settings_helper_SOURCES =						\
	main.c								\
	accessx.c							\
	accessx.h							\
	xkb.c								\
	xkb.h								\
	pointers.c							\
	pointers.h

xfce4_settings_helper_CFLAGS =						\
	$(GTK_CFLAGS)							\
	$(GLIB_CFLAGS)							\
	$(GTHREAD_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(XI_CFLAGS)							\
	$(LIBX11_CFLAGS)

xfce4_settings_helper_LDFLAGS =						\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

xfce4_settings_helper_LDADD =						\
	$(GTK_LIBS)							\
	$(GLIB_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(DBUS_GLIB_LIBS)						\
	$(XFCONF_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(XI_LIBS)							\
	$(LIBX11_LIBS)

if HAVE_LIBNOTIFY
xfce4_settings_helper_CFLAGS +=						\
	$(LIBNOTIFY_CFLAGS)

xfce4_settings_helper_LDADD +=						\
	$(LIBNOTIFY_LIBS)
endif

if HAVE_HAL
xfce4_settings_helper_CFLAGS +=						\
	$(HAL_CFLAGS)

xfce4_settings_helper_LDADD +=						\
	$(HAL_LIBS)
endif

clean-local:
	rm -f *.core core core.*

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
