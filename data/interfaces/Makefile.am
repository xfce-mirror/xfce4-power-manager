interfaces =					\
	xfpm-settings.ui			\
	acme.ui

interfaces_h =					\
	$(interfaces:.ui=_ui.h)

if MAINTAINER_MODE

BUILT_SOURCES = $(interfaces_h)

xfpm-settings_ui.h: xfpm-settings.ui
	exo-csource --static --strip-comments --strip-content --name=xfpm_settings_ui $< > $@

acme_ui.h: acme.ui
	exo-csource --static --strip-comments --strip-content --name=acme_ui $< > $@

endif

CLEANFILES =					\
	$(BUILT_SOURCES)

EXTRA_DIST =					\
	$(interfaces)				\
	$(interfaces_h)
