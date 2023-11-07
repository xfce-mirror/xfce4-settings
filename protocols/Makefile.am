if ENABLE_WAYLAND

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libprotocols\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libprotocols.la

libprotocols_built_sources = \
	wlr-output-management-unstable-v1.c \
	wlr-output-management-unstable-v1-client.h

nodist_libprotocols_la_SOURCES = \
	wlr-output-management-unstable-v1.c

libprotocols_la_CFLAGS = \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(PLATFORM_CFLAGS)

libprotocols_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libprotocols_la_LIBADD = \
	$(WAYLAND_CLIENT_LIBS)

%.c: $(WLR_PROTOCOLS_PKGDATADIR)/unstable/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WLR_PROTOCOLS_PKGDATADIR)/unstable/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@

DISTCLEANFILES = \
	$(libprotocols_built_sources)

BUILT_SOURCES = \
	$(libprotocols_built_sources)

endif # ENABLE_WAYLAND

EXTRA_DIST = \
	wlr-protocols/unstable/wlr-output-management-unstable-v1.xml
