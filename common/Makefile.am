# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

noinst_LTLIBRARIES = 							\
	libcommon.la

libcommon_la_SOURCES =							\
	xfce-shortcuts-provider.h					\
	xfce-shortcuts-provider.c					\
	xfce-shortcuts-grabber.h					\
	xfce-shortcuts-grabber.c

libcommon_la_CFLAGS = 							\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	$(PLATFORM_CFLAGS)						\
	$(GLIB_CFLAGS)							\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(XFCONF_CFLAGS)

libcommon_la_LDFLAGS =							\
	-no-undefined

libcommon_la_LIBADD =							\
	$(PLATFORM_LDFLAGS)						\
	$(GLIB_LIBS)							\
	$(GTK_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(XFCONF_LIBS)
