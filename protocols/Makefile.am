if ENABLE_WAYLAND

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libprotocols\" \
	$(PLATFORM_CPPFLAGS)

noinst_LTLIBRARIES = \
	libprotocols.la

libprotocols_built_sources = \
	ext-idle-notify-v1.c \
	ext-idle-notify-v1-client.h \
	wlr-output-power-management-unstable-v1.c \
	wlr-output-power-management-unstable-v1-client.h

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

# do not extract the WLR_PROTOCOLS_PKGDATADIR part from the if-else below: it generates warnings…
if HAVE_IDLE_NOTIFY
%.c: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-idle-notify/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WAYLAND_PROTOCOLS_PKGDATADIR)/staging/ext-idle-notify/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@

%.c: $(WLR_PROTOCOLS_PKGDATADIR)/unstable/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WLR_PROTOCOLS_PKGDATADIR)/unstable/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@
else
%.c: %.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: %.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@

%.c: $(WLR_PROTOCOLS_PKGDATADIR)/unstable/%.xml
	$(AM_V_GEN) wayland-scanner private-code $< $@

%-client.h: $(WLR_PROTOCOLS_PKGDATADIR)/unstable/%.xml
	$(AM_V_GEN) wayland-scanner client-header $< $@
endif

DISTCLEANFILES = \
	$(libprotocols_built_sources)

BUILT_SOURCES = \
	$(libprotocols_built_sources)

endif # ENABLE_WAYLAND

EXTRA_DIST =

if !HAVE_IDLE_NOTIFY
EXTRA_DIST += \
	ext-idle-notify-v1.xml
endif

EXTRA_DIST += \
	wlr-protocols/unstable/wlr-output-power-management-unstable-v1.xml
