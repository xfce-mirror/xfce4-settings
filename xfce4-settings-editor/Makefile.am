
INCLUDES = \
	-I${top_srcdir} \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-settings-editor\" \
	$(PLATFORM_CPPFLAGS)

bin_PROGRAMS = \
	xfce4-settings-editor

xfce4_settings_editor_built_sources = \
	xfce-settings-marshal.c \
	xfce-settings-marshal.h

xfce4_settings_editor_SOURCES = \
	$(xfce4_settings_editor_built_sources) \
	main.c \
	xfce-settings-cell-renderer.c \
	xfce-settings-cell-renderer.h \
	xfce-settings-editor-dialog.c \
	xfce-settings-editor-dialog.h \
	xfce-settings-prop-dialog.c \
	xfce-settings-prop-dialog.h

xfce4_settings_editor_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_settings_editor_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

xfce4_settings_editor_LDADD = \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-settings-editor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

#
# Build sources
#
if MAINTAINER_MODE

xfce-settings-marshal.h: xfce-settings-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=_xfce_settings_marshal --internal --header $< > $@

xfce-settings-marshal.c: xfce-settings-marshal.list Makefile
	$(AM_V_GEN) echo "#include \"xfce-settings-marshal.h\"" > $@ \
	&& glib-genmarshal --prefix=_xfce_settings_marshal --body $< >> $@

DISTCLEANFILES = \
	$(xfce4_settings_editor_built_sources)

BUILT_SOURCES = \
	$(xfce4_settings_editor_built_sources)

endif

EXTRA_DIST = \
	xfce-settings-marshal.list \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
