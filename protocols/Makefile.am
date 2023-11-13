if ENABLE_WAYLAND

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libprotocols\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libprotocols.la

libprotocols_built_sources = \
	ext-idle-notify-v1.c \
	ext-idle-notify-v1-client.h

nodist_libprotocols_la_SOURCES = \
	$(libprotocols_built_sources)

libprotocols_la_CFLAGS = \
	$(WAYLAND_CLIENT_CFLAGS) \
	$(PLATFORM_CFLAGS)

libprotocols_la_LDFLAGS = \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

libprotocols_la_LIBADD = \
	$(WAYLAND_CLIENT_LIBS)

%.c: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-idle-notify/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-idle-notify/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@

DISTCLEANFILES = \
	$(libprotocols_built_sources)

BUILT_SOURCES = \
	$(libprotocols_built_sources)

endif # ENABLE_WAYLAND

EXTRA_DIST =
