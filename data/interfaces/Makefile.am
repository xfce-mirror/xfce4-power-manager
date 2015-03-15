interfaces =					\
	xfpm-settings.ui

interfaces_h =					\
	$(interfaces:.ui=_ui.h)

if MAINTAINER_MODE

BUILT_SOURCES = $(interfaces_h)

xfpm-settings_ui.h: xfpm-settings.ui
	exo-csource --static --strip-comments --strip-content --name=xfpm_settings_ui $< > $@

endif

CLEANFILES =					\
	$(BUILT_SOURCES)

EXTRA_DIST =					\
	$(interfaces)				\
	$(interfaces_h)
